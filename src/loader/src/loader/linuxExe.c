#ifdef __linux__
#include <stdlib.h>
#include <unistd.h>

#include "log/log.h"
#include "mainShared.h"

uint32_t partialElfCrc = 0;

int main(int argc, char *argv[])
{
    char command[MAX_PATH_LENGTH] = {0};
    char originalDir[MAX_PATH_LENGTH] = {0};
    char gameELF[MAX_PATH_LENGTH] = {0};
    char libraryPath[MAX_PATH_LENGTH] = {0};

    if (parseArgs(argc, argv, command, originalDir, gameELF, libraryPath) != PARSE_ARGS_SUCCESS)
        return EXIT_SUCCESS;

    log_info("Starting $ %s", command);

    int sysCmd = system(command);

    if (chdir(originalDir) != 0)
    {
        log_error("Could not return to the original directory.");
        return EXIT_FAILURE;
    }

    return sysCmd;
}
#endif