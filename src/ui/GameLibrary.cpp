#include "GameLibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

#include "AppSettings.h"
#include "Bindings.h"
#include "ControlLayouts.h"
#include "DiskImage.h"
#include "IniFile.h"

static QSettings uiSettings(const QString &appDir)
{
    // The app used to be called LindLoader; keep its settings.
    const QString path = appDir + "/Lindy.ini";
    if (!QFileInfo::exists(path))
        QFile::rename(appDir + "/LindLoader.ini", path);
    return QSettings(path, QSettings::IniFormat);
}

// Games such as House of the Dead 4 read their data from ../fs. Dumps split by partition keep it in a
// sibling folder named like the game's instead (hod4prog + hod4data): that folder, or an empty string.
static QString splitDataFolder(const QString &gameDir)
{
    QDir parent(gameDir);
    const QString name = QFileInfo(gameDir).fileName();
    if (!parent.cdUp())
        return {};
    QStringList data;
    for (const QString &d : parent.entryList({"*data"}, QDir::Dirs | QDir::NoDotAndDotDot))
        if (d.size() > 4 && name.startsWith(d.chopped(4), Qt::CaseInsensitive))
            data << d;
    return data.size() == 1 ? parent.filePath(data.first()) : QString();
}

// A junction named fs lets the game find a split dump's data.
static void linkDataFolder(const QString &gameDir)
{
    QDir parent(gameDir);
    const QString data = splitDataFolder(gameDir);
    if (data.isEmpty() || !parent.cdUp() || parent.exists("fs"))
        return;
    QProcess::execute("cmd", {"/c", "mklink", "/J", QDir::toNativeSeparators(parent.filePath("fs")),
                              QDir::toNativeSeparators(data)});
}

GameLibrary::GameLibrary(const QString &appDir, QObject *parent) : QObject(parent), m_appDir(appDir)
{
    m_folders = uiSettings(m_appDir).value("library/folders").toStringList();
}

QString GameLibrary::loaderPath() const
{
    return m_appDir + "/linuxloader.exe";
}

void GameLibrary::addFolder(const QString &folder)
{
    const QString f = QDir::cleanPath(folder);
    if (!m_folders.contains(f, Qt::CaseInsensitive))
    {
        m_folders << f;
        saveFolders();
    }
}

void GameLibrary::removeFolder(const QString &folder)
{
    m_folders.removeAll(folder);
    saveFolders();
}

void GameLibrary::saveFolders() const
{
    QSettings s = uiSettings(m_appDir);
    s.setValue("library/folders", m_folders);
}

QVariant GameLibrary::setting(const QString &key, const QVariant &defaultValue) const
{
    return uiSettings(m_appDir).value(key, defaultValue.isValid() ? defaultValue : AppSettings::defaultValue(key));
}

void GameLibrary::removeSetting(const QString &key)
{
    QSettings s = uiSettings(m_appDir);
    s.remove(key);
}

QString GameLibrary::coversDir() const
{
    const QString dir = setting("folders/covers").toString();
    return dir.isEmpty() ? m_appDir + "/covers" : dir;
}

void GameLibrary::setSetting(const QString &key, const QVariant &value)
{
    QSettings s = uiSettings(m_appDir);
    s.setValue(key, value);
}

// Settings > Game List > Excluded Paths: a folder excludes the games below it, a file the game it is.
bool GameLibrary::isExcluded(const Game &game, const QStringList &excluded)
{
    const QString dir = QDir::cleanPath(game.gameDir);
    const QString elf = QDir::cleanPath(QDir(dir).absoluteFilePath(game.elf));
    for (const QString &e : excluded)
    {
        const QString path = QDir::cleanPath(e);
        if (path.compare(elf, Qt::CaseInsensitive) == 0 || path.compare(dir, Qt::CaseInsensitive) == 0 ||
            dir.startsWith(path + "/", Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// A short id for the game's folder: paths can't be INI keys.
QString GameLibrary::gameKey(const Game &game)
{
    const QByteArray dir = QDir::cleanPath(game.gameDir).toLower().toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(dir, QCryptographicHash::Sha1).toHex().left(16));
}

bool GameLibrary::isFavorite(const Game &game) const
{
    return setting("favorites/list").toStringList().contains(gameKey(game));
}

void GameLibrary::setFavorite(const Game &game, bool favorite)
{
    QStringList list = setting("favorites/list").toStringList();
    list.removeAll(gameKey(game));
    if (favorite)
        list << gameKey(game);
    setSetting("favorites/list", list);
}

qint64 GameLibrary::playSeconds(const Game &game) const
{
    return setting("played/" + gameKey(game) + "_seconds").toLongLong();
}

QDateTime GameLibrary::lastPlayed(const Game &game) const
{
    return setting("played/" + gameKey(game) + "_last").toDateTime();
}

void GameLibrary::recordPlay(const Game &game, qint64 seconds)
{
    const QString key = "played/" + gameKey(game);
    setSetting(key + "_seconds", playSeconds(game) + seconds);
    setSetting(key + "_last", QDateTime::currentDateTime());
}

// A game keeps its settings next to it when its folder already has a linuxloader.ini (a
// self-contained game folder: <folder>/linuxloader.ini with the game below it). Otherwise they
// live in Lindy's own config/<game> folder.
QString GameLibrary::findConfigDir(const QString &gameDir, const QString &libraryFolder, const QString &shortTitle) const
{
    QDir dir(gameDir);
    const QString root = QDir::cleanPath(libraryFolder);
    for (int up = 0; up < 5; ++up)
    {
        if (QFileInfo::exists(dir.filePath("linuxloader.ini")))
            return dir.absolutePath();
        if (QDir::cleanPath(dir.absolutePath()).compare(root, Qt::CaseInsensitive) == 0 || !dir.cdUp())
            break;
    }
    return m_appDir + "/config/" + shortTitle;
}

void GameLibrary::rescan()
{
    const QStringList folders = m_folders;
    const QStringList excluded = setting("library/excluded").toStringList();
    const QString loader = loaderPath();
    auto *watcher = new QFutureWatcher<QList<Game>>(this);
    connect(watcher, &QFutureWatcher<QList<Game>>::finished, this, [this, watcher] {
        m_games = watcher->result();
        std::sort(m_games.begin(), m_games.end(),
                  [](const Game &a, const Game &b) { return a.title.compare(b.title, Qt::CaseInsensitive) < 0; });
        watcher->deleteLater();
        emit scanned();
    });
    watcher->setFuture(QtConcurrent::run([this, folders, excluded, loader] {
        QList<Game> found;
        for (const QString &folder : folders)
            extractImages(folder);
        for (const QString &folder : folders)
        {
            QProcess p;
            p.setWorkingDirectory(QFileInfo(loader).absolutePath());
            p.start(loader, {"--identify", folder});
            p.waitForFinished(120000);
            const QList<QByteArray> lines = p.readAllStandardOutput().split('\n');
            for (const QByteArray &line : lines)
            {
                const QJsonObject o = QJsonDocument::fromJson(line.trimmed()).object();
                if (o.isEmpty())
                    continue;
                Game g;
                g.gameDir = QDir::cleanPath(o["dir"].toString());
                g.elf = o["elf"].toString();
                g.crc = o["crc"].toString();
                g.clean = o["clean"].toBool();
                g.known = o["known"].toBool();
                g.title = g.known ? o["title"].toString() : QStringLiteral("Unknown game (%1)").arg(g.elf);
                g.shortTitle = g.known ? o["short"].toString() : g.crc.toLower();
                g.gameId = o["id"].toString();
                g.dvp = o["dvp"].toString();
                g.year = o["year"].toString();
                g.resolutions = o["resolutions"].toString();
                g.type = o["type"].toString();
                g.configDir = findConfigDir(g.gameDir, folder, g.shortTitle);
                if (isExcluded(g, excluded))
                    continue;
                for (const QString &dir : {g.gameDir, splitDataFolder(g.gameDir)})
                    for (QDirIterator it(dir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
                         !dir.isEmpty() && it.hasNext();)
                        g.size += it.nextFileInfo().size();
                found << g;
            }
        }
        return found;
    }));
}

bool GameLibrary::ensureConfig(const Game &game, QString *error)
{
    QDir().mkpath(game.configDir);
    const struct
    {
        QString path, kind, name;
    } files[] = {{game.configPath(), "config", "linuxloader.ini"}, {game.controlsPath(), "controls", "controls.ini"}};
    for (const auto &f : files)
    {
        if (QFileInfo::exists(f.path))
            continue;
        if (!createDefaults(f.kind, game.configDir, f.name))
        {
            if (error)
                *error = QStringLiteral("Could not create %1").arg(f.path);
            return false;
        }
        if (f.kind == "controls")
            applyStarterControls(game.type, f.path);
    }
    return true;
}

bool GameLibrary::createDefaults(const QString &kind, const QString &dir, const QString &name) const
{
    QProcess p;
    p.setWorkingDirectory(m_appDir);
    p.start(loaderPath(), {"--create", kind, QDir::toNativeSeparators(dir), name});
    p.waitForFinished(30000);
    return QFileInfo::exists(dir + "/" + name);
}

namespace
{
// Moves everything in from into to, merging folders that exist in both.
void mergeInto(const QString &from, const QString &to)
{
    QDir().mkpath(to);
    for (const QFileInfo &fi : QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
    {
        const QString target = to + "/" + fi.fileName();
        if (fi.isDir() && QFileInfo(target).isDir())
            mergeInto(fi.filePath(), target);
        else
        {
            QFile::remove(target);
            QDir().rename(fi.filePath(), target);
        }
    }
    QDir(from).removeRecursively();
}
} // namespace

// Disk images (Batocera's .bin files) are unpacked once into a folder next to them, named after
// the image; the loader then finds the game there like any other. Some games come as several
// images, like the installer at github.com/zognic/lindbergh-install handles: "<game>_<part>.bin"
// belongs to "<game>.bin". When the part holds game folders (rom, disk0, ...) they are merged into
// the game's folder, otherwise it goes into a subfolder named after the part (harley_fs2.bin -> fs).
void GameLibrary::extractImages(const QString &folder)
{
    // Images sit in the library folder or one level below; game dumps themselves contain .bin files.
    QList<QFileInfo> images;
    const QStringList patterns = {"*.bin", "*.img", "*.ext2", "*.ext3", "*.ext4"};
    QList<QDir> dirs = {QDir(folder)};
    for (const QFileInfo &sub : QDir(folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        dirs << QDir(sub.filePath());
    for (const QDir &dir : dirs)
        for (const QFileInfo &image : dir.entryInfoList(patterns, QDir::Files))
            if (DiskImage::isImage(image.filePath()))
                images << image;
    // Whole games first, then their extra parts.
    auto partOf = [&images](const QFileInfo &image) -> QFileInfo {
        const QString name = image.completeBaseName();
        const int us = name.lastIndexOf('_');
        if (us > 0)
            for (const QFileInfo &other : images)
                if (other.absolutePath() == image.absolutePath() && other.completeBaseName() == name.left(us))
                    return other;
        return QFileInfo();
    };
    QList<QPair<QFileInfo, QFileInfo>> work; // image, the main image it is a part of (or empty)
    for (const QFileInfo &image : images)
        work << qMakePair(image, partOf(image));
    std::stable_sort(work.begin(), work.end(), [](const auto &a, const auto &b) {
        return a.second.fileName().isEmpty() && !b.second.fileName().isEmpty();
    });
    // An unpacked game folder holds a ".unpacked-<image>" marker. It is named after the image, or
    // "<name> (bin)" when a folder of that name already exists (Windows ignores case: vf5fs = VF5FS).
    auto folderFor = [](const QFileInfo &image) {
        const QString base = image.absolutePath() + "/" + image.completeBaseName();
        for (const QString &dir : {base, base + " (bin)"})
            if (QFileInfo::exists(dir + "/.unpacked-" + image.fileName()))
                return dir;
        return QFileInfo::exists(base) ? base + " (bin)" : base;
    };

    for (const auto &[image, main] : work)
    {
        const bool isPart = !main.fileName().isEmpty();
        const QString gameDir = folderFor(isPart ? main : image);
        const QString marker = gameDir + "/.unpacked-" + image.fileName();
        if (QFileInfo::exists(marker))
            continue;
        if (isPart && !QFileInfo::exists(gameDir + "/.unpacked-" + main.fileName()))
            continue; // the main image failed to unpack
        if (!isPart && QFileInfo::exists(gameDir))
            continue; // both names taken by other folders
        const QString partial = image.absolutePath() + "/" + image.completeBaseName() + ".partial";
        QDir(partial).removeRecursively();
        int lastPercent = -1;
        QString error;
        const bool ok = DiskImage::extract(image.filePath(), partial, [&](qint64 done, qint64 total) {
            const int percent = total ? int(done * 100 / total) : 100;
            if (percent != lastPercent)
            {
                lastPercent = percent;
                QMetaObject::invokeMethod(this, [this, name = image.fileName(), percent] { emit extracting(name, percent); });
            }
        }, &error);
        if (!ok)
        {
            QDir(partial).removeRecursively();
            QMetaObject::invokeMethod(this, [this, name = image.fileName(), error] { emit extractFailed(name, error); });
            continue;
        }
        if (!isPart)
        {
            QDir().rename(partial, gameDir);
            QFile f(marker);
            f.open(QIODevice::WriteOnly);
            continue;
        }
        const QDir top(partial);
        bool gameFolders = false;
        for (const QString &d : top.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            gameFolders |= d == "rom" || d.startsWith("disk") || QFileInfo(gameDir + "/" + d).isDir();
        QString sub = image.completeBaseName().mid(main.completeBaseName().size() + 1);
        while (!sub.isEmpty() && sub.back().isDigit())
            sub.chop(1);
        if (!gameFolders)
            mergeInto(partial, gameDir + "/" + sub);
        else
        {
            // Each folder joins the game's folder of that name, which can sit one level down
            // (vf5fs_ext.bin's rom belongs in disk1/rom). Batocera copies the disks over each
            // other in order, so like there the last disk with that folder wins.
            for (const QFileInfo &fi : top.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
            {
                QString target = gameDir + "/" + fi.fileName();
                if (fi.isDir() && !QFileInfo(target).isDir())
                    for (const QFileInfo &disk : QDir(gameDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
                        if (QFileInfo(disk.filePath() + "/" + fi.fileName()).isDir())
                            target = disk.filePath() + "/" + fi.fileName();
                if (fi.isDir())
                    mergeInto(fi.filePath(), target);
                else
                {
                    QFile::remove(target);
                    QDir().rename(fi.filePath(), target);
                }
            }
            QDir(partial).removeRecursively();
        }
        QFile f(marker);
        f.open(QIODevice::WriteOnly);
    }
}

// Linux Loader's default controls only cover player 1's controller; fill in both players.
void GameLibrary::applyStarterControls(const QString &type, const QString &path) const
{
    IniFile ini;
    if (!ini.load(path))
        return;
    ControlLayouts::applyStarterBindings(ini, type);
    ini.save();
}


bool GameLibrary::applySettings(const Game &game, QString *error)
{
    if (!ensureConfig(game, error))
        return false;
    linkDataFolder(game.gameDir);
    auto quoted = [](const QString &value) { return "\"" + value + "\""; };

    IniFile config;
    if (!config.load(game.configPath()))
    {
        if (error)
            *error = "Could not read " + QDir::toNativeSeparators(game.configPath());
        return false;
    }

    // Settings > Audio
    config.setValue("Audio", "AUDIO_DEVICE", quoted(setting("audio/device").toString()));
    config.setValue("Audio", "AUDIO_VOLUME",
                    QString::number(setting("audio/mute").toBool() ? 0 : setting("audio/volume").toInt()));

    // Controller Settings > Raw Input: one gun per mouse
    config.setValue("Input", "RAW_MOUSE_INPUT", setting("input/rawMice").toBool() ? "true" : "false");

    // Settings > Network
    const bool network = setting("network/enabled").toBool();
    config.setValue("Network", "ENABLE_NETWORK_PATCHES", network ? "true" : "false");
    if (network)
    {
        const struct
        {
            const char *iniKey, *setting;
        } addresses[] = {{"OR2_IPADDRESS", "or2Ip"}, {"OR2_NETMASK", "or2Netmask"}, {"ID_IP_SEAT_1", "idSeat1"},
                         {"ID_IP_SEAT_2", "idSeat2"},  {"IP_CAB1", "cab1"},          {"IP_CAB2", "cab2"},
                         {"IP_CAB3", "cab3"},          {"IP_CAB4", "cab4"},          {"2SPICY_IP_CAB1", "spicy1"},
                         {"2SPICY_IP_CAB2", "spicy2"}, {"SRTV_IPADDRESS", "srtvIp"}};
        for (const auto &a : addresses)
            config.setValue("Network", a.iniKey, quoted(setting(QString("network/") + a.setting).toString()));
    }

    // Settings > Folders: save data and card files, in a folder per game or (by default) the game's own.
    // Moving to a folder takes the game's existing files along the first time.
    auto place = [&](const QString &folderSetting, const QString &section, const QString &key, const QString &file) {
        const QString folder = setting(folderSetting).toString();
        if (folder.isEmpty())
            return config.setValue(section, key, quoted(file));
        const QString dir = QDir::cleanPath(folder + "/" + game.shortTitle);
        QDir().mkpath(dir);
        if (!file.isEmpty())
        {
            const QString target = dir + "/" + file;
            const QString old = QDir(game.gameDir).filePath(file);
            if (!QFileInfo::exists(target) && QFileInfo::exists(old))
                QFile::copy(old, target);
            config.setValue(section, key, quoted(target));
        }
        else
        {
            config.setValue(section, key, quoted(dir));
        }
    };
    place("folders/saves", "Paths", "SRAM_PATH", "sram.bin");
    place("folders/saves", "Paths", "EEPROM_PATH", "eeprom.bin");
    place("folders/cards", "Cards", "CARDFILE_01", "Card_01.crd");
    place("folders/cards", "Cards", "CARDFILE_02", "Card_02.crd");
    if (!setting("folders/cards").toString().isEmpty())
        place("folders/cards", "Cards", "ID_CARDFOLDER", QString());
    else
        config.setValue("Cards", "ID_CARDFOLDER", quoted(QString()));

    // Controller Settings > Crosshairs (the loader draws them in HOTD4, Rambo, Ghost Squad Evolution and Primeval Hunt)
    config.setValue("CrossHairs", "ENABLE_CROSSHAIRS", setting("crosshairs/enabled").toBool() ? "true" : "false");
    config.setValue("CrossHairs", "P1_CROSSHAIR_PATH", quoted(crosshairPath(1)));
    config.setValue("CrossHairs", "P2_CROSSHAIR_PATH", quoted(crosshairPath(2)));
    const QString size = QString::number(setting("crosshairs/size").toInt());
    config.setValue("CrossHairs", "CUSTOM_CROSSHAIRS_WIDTH", size);
    config.setValue("CrossHairs", "CUSTOM_CROSSHAIRS_HEIGHT", size);

    // Controller Settings > Hotkeys
    IniFile controls;
    if (!config.save() || !controls.load(game.controlsPath()))
    {
        if (error)
            *error = "Could not update the settings in " + QDir::toNativeSeparators(game.configDir);
        return false;
    }
    for (const AppSettings::Hotkey &h : AppSettings::hotkeys())
        controls.setValue("Common", h.key, setting(QString("hotkeys/") + h.key).toString());
    if (!controls.save())
    {
        if (error)
            *error = "Could not save " + QDir::toNativeSeparators(game.controlsPath());
        return false;
    }
    return true;
}

QString GameLibrary::crosshairPath(int player) const
{
    const QString chosen = setting(QStringLiteral("crosshairs/p%1").arg(player)).toString();
    if (!chosen.isEmpty())
        return QDir::cleanPath(chosen);
    // The loader reads files, not Lindy's resources: keep a copy of the default.
    const QString path = QStringLiteral("%1/crosshairs/p%2.png").arg(m_appDir).arg(player);
    if (!QFileInfo::exists(path))
    {
        QDir().mkpath(m_appDir + "/crosshairs");
        QFile::copy(QStringLiteral(":/icons/crosshair-p%1.png").arg(player), path);
        QFile(path).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }
    return path;
}

QStringList GameLibrary::launchEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    auto flag = [&](const char *hint, const char *settingKey) { env.insert(hint, setting(settingKey).toBool() ? "1" : "0"); };
    // Controller Settings > Global Settings (SDL hints, read by the loader's SDL at start)
    flag("SDL_JOYSTICK_RAWINPUT", "input/sdlRawInput");
    flag("SDL_JOYSTICK_ENHANCED_REPORTS", "input/enhancedMode");
    flag("SDL_JOYSTICK_HIDAPI", "input/hidapi");
    flag("SDL_XINPUT_ENABLED", "input/xinput");
    flag("SDL_JOYSTICK_DIRECTINPUT", "input/directinput");
    // Settings > Interface: games keep the screensaver off unless it's allowed
    env.insert("SDL_VIDEO_ALLOW_SCREENSAVER", setting("interface/inhibitScreensaver").toBool() ? "0" : "1");
    // Settings > Audio
    if (const QString driver = setting("audio/driver").toString(); !driver.isEmpty())
        env.insert("SDL_AUDIO_DRIVER", driver);
    if (const int ms = setting("audio/bufferMs").toInt(); ms > 0)
        env.insert("SDL_AUDIO_DEVICE_SAMPLE_FRAMES", QString::number(ms * 48)); // at 48 kHz
    return env.toStringList();
}

int GameLibrary::linkCabinets(const Game &game)
{
    // Sega Race TV: up to four cabinets (1 Main, 2-4 Sub).
    return game.gameId == "SBPF" ? 4 : 0;
}

// Sega Race TV keeps its cabinet ID (0 Main, 1-3 Sub, 4 stand-alone) in two copies of a 72-byte
// EEPROM record, each starting with a CRC32 (without the final inversion) of the 68 bytes after it.
static bool setSrtvCabinet(const QString &eeprom, int id)
{
    QFile f(eeprom);
    if (!f.open(QIODevice::ReadWrite))
        return false;
    QByteArray data = f.readAll();
    static quint32 table[256];
    if (!table[1])
        for (quint32 i = 0; i < 256; ++i)
        {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    for (int base : {0x1140, 0x11C0})
    {
        if (data.size() < base + 72 || !data.mid(base + 4, 4).startsWith("R_TV"))
            return false;
        data[base + 0x18] = char(id);
        quint32 crc = 0xFFFFFFFFu;
        for (int i = base + 4; i < base + 72; ++i)
            crc = table[(crc ^ quint8(data[i])) & 0xFF] ^ (crc >> 8);
        for (int b = 0; b < 4; ++b)
            data[base + b] = char(crc >> (8 * b));
    }
    f.seek(0);
    return f.write(data) == data.size();
}

QString GameLibrary::prepareLinkCabinet(const Game &game, int cabinet, const QString &controllerGuid, bool sound,
                                        QString *error)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QString();
    };
    const QString dir = QStringLiteral("%1/config/%2-cabinet%3").arg(m_appDir, game.shortTitle).arg(cabinet);
    QDir().mkpath(dir);
    const QString config = dir + "/linuxloader.ini", controls = dir + "/controls.ini";
    QFile::remove(config);
    QFile::remove(controls);
    if (!QFile::copy(game.configPath(), config) || !QFile::copy(game.controlsPath(), controls))
        return fail("Could not copy the settings of " + game.title);

    IniFile ini;
    ini.load(config);
    // Where the game's own save data is: its EEPROM is the starting point of every cabinet's.
    QString eeprom = ini.value("Paths", "EEPROM_PATH", "eeprom.bin").remove('"');
    if (QFileInfo(eeprom).isRelative())
        eeprom = QDir(game.gameDir).filePath(eeprom);
    auto quoted = [](const QString &v) { return "\"" + v + "\""; };
    ini.setValue("Network", "ENABLE_NETWORK_PATCHES", "true");
    ini.setValue("Network", "LOOPBACK_LINK", "true");
    ini.setValue("Network", "SRTV_IPADDRESS", quoted(QString()));
    ini.setValue("Display", "FULLSCREEN", "false");
    ini.setValue("Paths", "SRAM_PATH", quoted(dir + "/sram.bin"));
    ini.setValue("Paths", "EEPROM_PATH", quoted(dir + "/eeprom.bin"));
    if (!sound)
        ini.setValue("Audio", "AUDIO_VOLUME", "0");
    if (!ini.save())
        return fail("Could not save " + QDir::toNativeSeparators(config));

    // The cabinet's own save data, its cabinet ID set (cabinet 1 is the Main one).
    const QString cabEeprom = dir + "/eeprom.bin";
    if (!QFileInfo::exists(cabEeprom) && !QFile::copy(eeprom, cabEeprom))
        return fail(game.title + " has no save data yet. Play it once on its own first, so it sets it up.");
    QFile(cabEeprom).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (!setSrtvCabinet(cabEeprom, cabinet - 1))
        return fail("Could not set the cabinet ID in " + QDir::toNativeSeparators(cabEeprom));

    // Player 1 of cabinet n plays with player slot n's controller: swap the GC0/JOY0 bindings with GCn-1/JOYn-1.
    IniFile pads;
    pads.load(controls);
    const QString a = "#0_", b = QStringLiteral("#%1_").arg(cabinet - 1);
    for (const QString &section : {"Common", "Digital", "Driving", "Shooting", "Flying", "Mahjong", "Config"})
        for (const QString &key : cabinet > 1 ? pads.keys(section) : QStringList())
        {
            QString v = pads.value(section, key, QString());
            for (const char *kind : {"GC", "JOY"})
            {
                const QString from = QString(a).replace('#', kind), to = QString(b).replace('#', kind);
                v.replace(from, "@@").replace(to, from).replace("@@", to);
            }
            pads.setValue(section, key, v);
        }
    // The keyboard plays only the keyboard cabinet (the one whose window has the focus), controllers only the
    // others; the hotkeys stay everywhere.
    const bool keyboard = controllerGuid == KeyboardController;
    static const QRegularExpression controller("\\b(GC|JOY)\\d"), key("\\bKEY_");
    QStringList hotkeys;
    for (const AppSettings::Hotkey &h : AppSettings::hotkeys())
        hotkeys << h.key;
    for (const QString &section : {"Common", "Digital", "Driving", "Shooting", "Flying", "Mahjong"})
        for (const QString &k : pads.keys(section))
        {
            if (hotkeys.contains(k))
                continue;
            QStringList kept;
            for (const QString &b : Bindings::split(pads.value(section, k, QString())))
                if (!b.contains(keyboard ? controller : key))
                    kept << b;
            pads.setValue(section, k, kept.join(", "));
        }
    for (int p = 1; p <= 4; ++p)
        pads.removeValue("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(p));
    if (!controllerGuid.isEmpty() && !keyboard)
        pads.setValue("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(cabinet), controllerGuid);
    if (!pads.save())
        return fail("Could not save " + QDir::toNativeSeparators(controls));
    return dir;
}
