#if defined(_WIN32) || defined(__MINGW32__)
#include "regexBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
#include <unordered_map>
#include <string>
#include <mutex>

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

namespace RegexBridge
{
    void initBridges()
    {
        log_info("Initializing Regex Bridges...");

        MAP("regcomp", bridgeRegcomp);
        MAP("regexec", bridgeRegexec);
        MAP("regfree", bridgeRegfree);

    }

    static std::unordered_map<void*, RegexData*> s_regexMap;
    static std::mutex s_regexMutex;

    extern "C" int __attribute__((cdecl)) bridgeRegcomp(void *preg, const char *pattern, int cflags)
    {
        std::lock_guard<std::mutex> lock(s_regexMutex);
        if (!preg || !pattern) {
            return L_REG_BADPAT;
        }

        try {
            std::regex_constants::syntax_option_type syntax = std::regex_constants::grep;
            if (cflags & L_REG_EXTENDED)
                syntax = std::regex_constants::egrep;
            if (cflags & L_REG_ICASE)
                syntax |= std::regex_constants::icase;

            RegexData* rd = new RegexData();
            rd->re.assign(pattern, syntax);
            rd->nosub = (cflags & L_REG_NOSUB) != 0;

            s_regexMap[preg] = rd;
            
            log_info("regcomp compiled regex: %s", pattern);
            return 0; // Success
        }
        catch (const std::regex_error& e) {
            log_error("regcomp failed for pattern: %s (error: %s)", pattern, e.what());
            return L_REG_BADPAT;
        }
    }

    extern "C" int __attribute__((cdecl)) bridgeRegexec(void *preg, const char *string, size_t nmatch, LinuxRegmatch *pmatch, int eflags)
    {
        std::lock_guard<std::mutex> lock(s_regexMutex);
        
        if (!preg || !string) {
            return L_REG_NOMATCH;
        }

        auto it = s_regexMap.find(preg);
        if (it == s_regexMap.end()) {
            log_error("regexec called with unknown preg: %p", preg);
            return L_REG_NOMATCH;
        }

        RegexData* rd = it->second;
        
        std::regex_constants::match_flag_type flags = std::regex_constants::match_default;
        if (eflags & L_REG_NOTBOL)
            flags |= std::regex_constants::match_not_bol;
        if (eflags & L_REG_NOTEOL)
            flags |= std::regex_constants::match_not_eol;

        std::cmatch matches;
        
        if (std::regex_search(string, matches, rd->re, flags)) {
            if (!rd->nosub && nmatch > 0 && pmatch != nullptr) {
                for (size_t i = 0; i < nmatch; ++i) {
                    if (i < matches.size() && matches[i].matched) {
                        pmatch[i].rm_so = std::distance(string, matches[i].first);
                        pmatch[i].rm_eo = std::distance(string, matches[i].second);
                    } else {
                        pmatch[i].rm_so = -1;
                        pmatch[i].rm_eo = -1;
                    }
                }
            }
            return 0; // Success (match found)
        }
        
        return L_REG_NOMATCH; // No match
    }

    extern "C" void __attribute__((cdecl)) bridgeRegfree(void *preg)
    {
        std::lock_guard<std::mutex> lock(s_regexMutex);
        if (!preg) return;

        auto it = s_regexMap.find(preg);
        if (it != s_regexMap.end()) {
            delete it->second;
            s_regexMap.erase(it);
            log_info("regfree freed regex at %p", preg);
        }
    }

} // namespace RegexBridge

#endif