#pragma once

#include <QMainWindow>

#include "GameLibrary.h"

class QAction;
class QLabel;
class QStackedWidget;
class QTimer;
class QTreeWidget;

// Laid out like DuckStation and PCSX2: menu bar, toolbar, game list table, status bar.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &appDir, QWidget *parent = nullptr);

private:
    void buildActions();
    void buildMenus();
    void buildToolBar();
    QWidget *buildEmptyState();

    void addFolder();
    void refreshList();
    void updateActions();
    void showProperties(int page);
    void openGameFolder();
    void showContextMenu(const QPoint &pos);
    void play();
    void watchRunning();

    const Game *currentGame() const;

    GameLibrary m_library;
    QStackedWidget *m_stack = nullptr;
    QTreeWidget *m_list = nullptr;
    QLabel *m_statusGames = nullptr;

    QAction *m_start = nullptr;
    QAction *m_addFolder = nullptr;
    QAction *m_rescan = nullptr;
    QAction *m_properties = nullptr;
    QAction *m_controls = nullptr;
    QAction *m_openFolder = nullptr;
    QAction *m_exit = nullptr;

    void *m_process = nullptr; // HANDLE of the running loader
    QString m_runningTitle;
    QTimer *m_runTimer = nullptr;
};
