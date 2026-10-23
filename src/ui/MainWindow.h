#pragma once

#include <QDateTime>
#include <QHash>
#include <QMainWindow>
#include <QPixmap>

#include "GameLibrary.h"

class QAbstractItemView;
class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QSlider;
class QStackedWidget;
class QStandardItemModel;
class QTimer;
class QToolButton;
class QTreeView;
class CoverDelegate;
class GameFilter;

// Laid out like DuckStation and PCSX2: menu bar, toolbar, game list bar, game list or grid, status bar.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &appDir, QWidget *parent = nullptr);
    // Fills the game list from the library folders.
    void scanLibrary();
    // For frontends (Lindy.exe [--linked <cabinets>] <game>): starts the game at path without showing
    // Lindy, linked with that many cabinets if more than one, and quits Lindy when it closes.
    // False if it couldn't start (Lindy has said why).
    bool runFromCommandLine(const QString &path, int cabinets);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolBar();
    QWidget *buildListBar();
    QWidget *buildEmptyState();

    void addFolder();
    void refreshList();
    void updateActions();
    void showProperties(int page);
    void showSettings(int page);
    void showControllerSettings(int page);
    void applyInterfaceSettings();
    void paintBackground(QWidget *viewport);
    void openGameFolder();
    void toggleFavorite();
    void showContextMenu(QAbstractItemView *view, const QPoint &pos);
    void play();
    void startLinkedPlay();
    void startLinkedCabinets(const Game &game, const QStringList &controllerGuids, bool soundFromFirstOnly);
    void *launch(const Game &game, const QString &configDir, const QStringList &extraEnvironment);
    void started(const Game &game, const QList<void *> &processes);
    void arrangeWindows();
    void watchRunning();

    void setGridView(bool grid);
    void updateGridSize();
    void fitTitleColumn();

    const Game *currentGame() const;
    QString coverPath(const Game &game) const;

    QString m_appDir;
    GameLibrary m_library;
    QStackedWidget *m_stack = nullptr;
    QStackedWidget *m_views = nullptr;
    QStandardItemModel *m_model = nullptr;
    GameFilter *m_filter = nullptr;
    QTreeView *m_list = nullptr;
    QListView *m_grid = nullptr;
    CoverDelegate *m_coverDelegate = nullptr;
    QLabel *m_statusGames = nullptr;

    QToolButton *m_listButton = nullptr;
    QToolButton *m_gridButton = nullptr;
    QToolButton *m_titlesButton = nullptr;
    QSlider *m_zoom = nullptr;
    QToolButton *m_favoritesButton = nullptr;
    QComboBox *m_typeFilter = nullptr;
    QLineEdit *m_search = nullptr;

    QAction *m_start = nullptr;
    QAction *m_addFolder = nullptr;
    QAction *m_rescan = nullptr;
    QAction *m_properties = nullptr;
    QAction *m_controls = nullptr;
    QAction *m_openFolder = nullptr;
    QAction *m_favorite = nullptr;
    QAction *m_linkedPlay = nullptr;
    QAction *m_settings = nullptr;
    QAction *m_controllerSettings = nullptr;
    QAction *m_exit = nullptr;

    QList<void *> m_processes; // HANDLEs of the running loaders (one per cabinet in linked play)
    QTimer *m_arrangeTimer = nullptr;
    QHash<int, int> m_arranged; // linked play: cabinet -> tick its window first appeared
    int m_arrangeTries = 0;
    int m_keyboardCabinet = -1; // linked play: the cabinet played with the keyboard, or -1
    Game m_runningGame;
    QDateTime m_runningSince;
    QTimer *m_runTimer = nullptr;
    bool m_commandLine = false; // started by a frontend: quit when the game closes

    QPixmap m_background; // Settings > Interface > Game List Background
};
