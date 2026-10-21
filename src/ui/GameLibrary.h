#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct Game
{
    QString title;       // "Virtua Fighter 5 Final Showdown REV B"
    QString shortTitle;  // "vf5-fs-rev-b"
    QString gameId;      // "SBXX"
    QString dvp;         // "DVP-5020"
    QString year;
    QString resolutions; // native resolutions, e.g. "640x480, 1360x768"
    QString gameDir;     // folder passed to the loader with -g
    QString elf;
    QString crc;
    bool clean = false;
    bool known = false;

    QString configDir;   // folder holding this game's linuxloader.ini and controls.ini
    QString configPath() const { return configDir + "/linuxloader.ini"; }
    QString controlsPath() const { return configDir + "/controls.ini"; }
};

// The games found in the library folders, identified by linuxloader.exe --identify.
class GameLibrary : public QObject
{
    Q_OBJECT
public:
    explicit GameLibrary(const QString &appDir, QObject *parent = nullptr);

    QString loaderPath() const;
    QStringList folders() const { return m_folders; }
    void addFolder(const QString &folder);
    void removeFolder(const QString &folder);
    const QList<Game> &games() const { return m_games; }

    // Rescans every library folder (runs in the background, emits scanned()).
    void rescan();

    // Makes sure the game has a linuxloader.ini and controls.ini, creating defaults if needed.
    bool ensureConfig(const Game &game, QString *error);

signals:
    void scanned();

private:
    void saveFolders() const;
    QString findConfigDir(const QString &gameDir, const QString &libraryFolder, const QString &shortTitle) const;

    QString m_appDir;
    QStringList m_folders;
    QList<Game> m_games;
};
