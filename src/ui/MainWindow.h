#pragma once

#include <QMainWindow>

#include "GameLibrary.h"
#include "IniFile.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &appDir, QWidget *parent = nullptr);

private:
    QWidget *buildLibraryPanel();
    QWidget *buildDetailPanel();
    QWidget *buildEmptyState();
    QWidget *settingsRow(const QString &label, QWidget *field, const QString &hint = QString());

    void addFolder();
    void refreshList();
    void showGame(int row);
    void loadSettings();
    void saveSetting(const QString &key, const QString &value);
    void play();
    void watchRunning();

    const Game *currentGame() const;

    GameLibrary m_library;
    IniFile m_ini;
    bool m_loading = false;

    QListWidget *m_list = nullptr;
    QLabel *m_count = nullptr;
    QStackedWidget *m_stack = nullptr;

    QLabel *m_title = nullptr;
    QLabel *m_meta = nullptr;
    QLabel *m_badge = nullptr;
    QLabel *m_path = nullptr;
    QPushButton *m_play = nullptr;
    QLabel *m_status = nullptr;

    QComboBox *m_resolution = nullptr;
    QComboBox *m_window = nullptr;
    QComboBox *m_vsync = nullptr;
    QCheckBox *m_aspect = nullptr;
    QCheckBox *m_fps = nullptr;

    void *m_process = nullptr; // HANDLE of the running loader
    QTimer *m_runTimer = nullptr;
};
