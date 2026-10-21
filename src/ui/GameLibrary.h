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
    QString type;        // control layout: "digital", "driving", "shooting", "flying" or "mahjong"
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

    // Writes the loader's default linuxloader.ini ("config") or controls.ini ("controls") to dir/name.
    bool createDefaults(const QString &kind, const QString &dir, const QString &name) const;

    // Fills in the starter bindings (both players, keyboard and controllers) of a new controls.ini.
    void applyStarterControls(const QString &type, const QString &path) const;

signals:
    void scanned();
    void extracting(const QString &image, int percent);
    void extractFailed(const QString &image, const QString &error);

private:
    void saveFolders() const;
    void extractImages(const QString &folder);
    QString findConfigDir(const QString &gameDir, const QString &libraryFolder, const QString &shortTitle) const;

    QString m_appDir;
    QStringList m_folders;
    QList<Game> m_games;
};
