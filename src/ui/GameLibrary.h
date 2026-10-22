#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

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
    qint64 size = 0;     // bytes in gameDir

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

    // Lindy.ini values, e.g. "ui/view". Without a defaultValue, AppSettings has the default.
    QVariant setting(const QString &key, const QVariant &defaultValue = {}) const;
    void setSetting(const QString &key, const QVariant &value);
    void removeSetting(const QString &key);

    QString appDir() const { return m_appDir; }
    QString coversDir() const;

    // Before a game starts: writes Lindy's settings that the loader reads (audio, network, save and
    // card folders, raw mouse input, crosshairs, hotkeys) into the game's linuxloader.ini and
    // controls.ini, and links a split dump's data folder where the game looks for it.
    bool applySettings(const Game &game, QString *error);
    // Linked play on this PC: several cabinets of a linked game, one loader each (LOOPBACK_LINK).
    // How many cabinets the game links, 0 if it can't.
    static int linkCabinets(const Game &game);
    // prepareLinkCabinet's controllerGuid for a cabinet played with the keyboard.
    static inline const QString KeyboardController = QStringLiteral("keyboard");
    // Settings for cabinet n (1-based): a copy of the game's with link play on, windowed, its own
    // save data with its cabinet ID set, and player 1 on the n-th controller (or on controllerGuid, or the keyboard).
    // Returns the folder holding its linuxloader.ini and controls.ini, or an empty string.
    QString prepareLinkCabinet(const Game &game, int cabinet, const QString &controllerGuid, bool sound,
                               QString *error);

    // A player's (1 or 2) crosshair image: the chosen PNG, or Lindy's own, copied next to Lindy.exe.
    QString crosshairPath(int player) const;
    // The environment to start the loader with: the system's, plus SDL's input, audio and screensaver hints.
    QStringList launchEnvironment() const;

    // Per game, kept in Lindy.ini by game folder.
    bool isFavorite(const Game &game) const;
    void setFavorite(const Game &game, bool favorite);
    qint64 playSeconds(const Game &game) const;
    QDateTime lastPlayed(const Game &game) const;
    void recordPlay(const Game &game, qint64 seconds);

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
    static QString gameKey(const Game &game);
    static bool isExcluded(const Game &game, const QStringList &excluded);
    void extractImages(const QString &folder);
    QString findConfigDir(const QString &gameDir, const QString &libraryFolder, const QString &shortTitle) const;

    QString m_appDir;
    QStringList m_folders;
    QList<Game> m_games;
};
