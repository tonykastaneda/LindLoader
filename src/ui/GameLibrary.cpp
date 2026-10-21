#include "GameLibrary.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

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
