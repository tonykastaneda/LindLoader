#pragma once

#include "SettingsWindow.h"

class QComboBox;
class QTreeWidget;

// Settings > Interface, Game List, Audio, Network and Folders: Lindy's own settings, the same for
// every game (each game's settings are in its Properties).
class SettingsDialog : public SettingsWindow
{
    Q_OBJECT
public:
    enum Page
    {
        Interface,
        GameList,
        Audio,
        Network,
        Folders,
    };
    SettingsDialog(GameLibrary &library, Page page = Interface, QWidget *parent = nullptr);

private:
    QWidget *buildInterfacePage();
    QWidget *buildGameListPage();
    QWidget *buildAudioPage();
    QWidget *buildNetworkPage();
    QWidget *buildFoldersPage();
    QWidget *folderRow(const QString &title, const QString &description, const QString &key,
                       const QString &defaultText);
    void fillAudioDevices();
    void rescan();

    QTreeWidget *m_folders = nullptr;
    QTreeWidget *m_excluded = nullptr;
    QComboBox *m_audioDriver = nullptr;
    QComboBox *m_audioDevice = nullptr;
};
