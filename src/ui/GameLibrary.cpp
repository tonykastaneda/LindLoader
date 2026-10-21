#include "GameLibrary.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

#include "DiskImage.h"

static QSettings uiSettings(const QString &appDir)
{
    return QSettings(appDir + "/LindLoader.ini", QSettings::IniFormat);
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

// A game keeps its settings next to it when its folder already has a linuxloader.ini (a
// self-contained game folder: <folder>/linuxloader.ini with the game below it). Otherwise they
// live in LindLoader's own config/<game> folder.
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
    const QString loader = loaderPath();
    auto *watcher = new QFutureWatcher<QList<Game>>(this);
    connect(watcher, &QFutureWatcher<QList<Game>>::finished, this, [this, watcher] {
        m_games = watcher->result();
        std::sort(m_games.begin(), m_games.end(),
                  [](const Game &a, const Game &b) { return a.title.compare(b.title, Qt::CaseInsensitive) < 0; });
        watcher->deleteLater();
        emit scanned();
    });
    watcher->setFuture(QtConcurrent::run([this, folders, loader] {
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
        if (!QFileInfo::exists(f.path) && !createDefaults(f.kind, game.configDir, f.name))
        {
            if (error)
                *error = QStringLiteral("Could not create %1").arg(f.path);
            return false;
        }
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
    std::stable_sort(images.begin(), images.end(), [&](const QFileInfo &a, const QFileInfo &b) {
        return partOf(a).fileName().isEmpty() && !partOf(b).fileName().isEmpty();
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

    for (const QFileInfo &image : images)
    {
        const QFileInfo main = partOf(image);
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
            gameFolders |= QFileInfo::exists(gameDir + "/" + d) || d == "rom" || d.startsWith("disk");
        QString sub = image.completeBaseName().mid(main.completeBaseName().size() + 1);
        while (!sub.isEmpty() && sub.back().isDigit())
            sub.chop(1);
        mergeInto(partial, gameFolders ? gameDir : gameDir + "/" + sub);
        QFile f(marker);
        f.open(QIODevice::WriteOnly);
    }
}
