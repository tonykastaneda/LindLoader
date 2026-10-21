#include "MainWindow.h"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <windows.h>

#include "GamePropertiesDialog.h"

namespace
{
const int RoleIndex = Qt::UserRole + 1;

enum Column
{
    ColTitle,
    ColId,
    ColDvp,
    ColYear,
    ColResolution,
    ColDump,
    ColCount
};
} // namespace

MainWindow::MainWindow(const QString &appDir, QWidget *parent) : QMainWindow(parent), m_library(appDir)
{
    setWindowTitle("LindLoader");
    setWindowIcon(QIcon(":/icons/play.svg"));
    resize(1100, 680);

    buildActions();
    buildMenus();
    buildToolBar();

    m_list = new QTreeWidget;
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->setSortingEnabled(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setHeaderLabels({"Title", "Game ID", "DVP", "Year", "Native Resolution", "Dump"});
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    m_list->setColumnWidth(ColId, 90);
    m_list->setColumnWidth(ColDvp, 100);
    m_list->setColumnWidth(ColYear, 60);
    m_list->setColumnWidth(ColResolution, 150);
    m_list->setColumnWidth(ColDump, 90);
    m_list->sortByColumn(ColTitle, Qt::AscendingOrder);

    m_stack = new QStackedWidget;
    m_stack->addWidget(buildEmptyState());
    m_stack->addWidget(m_list);
    setCentralWidget(m_stack);

    m_statusGames = new QLabel;
    statusBar()->addPermanentWidget(m_statusGames);

    connect(m_list, &QTreeWidget::itemDoubleClicked, this, &MainWindow::play);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &MainWindow::updateActions);
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(&m_library, &GameLibrary::scanned, this, &MainWindow::refreshList);
    connect(&m_library, &GameLibrary::extracting, this, [this](const QString &image, int percent) {
        statusBar()->showMessage(QStringLiteral("Unpacking %1... %2%").arg(image).arg(percent));
    });
    connect(&m_library, &GameLibrary::extractFailed, this, [this](const QString &image, const QString &error) {
        QMessageBox::warning(this, "LindLoader", QStringLiteral("Could not unpack %1: %2").arg(image, error));
    });

    m_runTimer = new QTimer(this);
    m_runTimer->setInterval(1000);
    connect(m_runTimer, &QTimer::timeout, this, &MainWindow::watchRunning);

    if (!QFileInfo::exists(m_library.loaderPath()))
        QMessageBox::warning(this, "LindLoader", "linuxloader.exe was not found next to LindLoader.exe.");
    updateActions();
    statusBar()->showMessage("Scanning game folders...");
    m_library.rescan();
}

void MainWindow::buildActions()
{
    m_start = new QAction(QIcon(":/icons/play.svg"), "&Start Game", this);
    m_start->setShortcut(QKeySequence(Qt::Key_Return));
    m_addFolder = new QAction(QIcon(":/icons/folder-add.svg"), "&Add Game Folder...", this);
    m_rescan = new QAction(QIcon(":/icons/refresh.svg"), "&Rescan Game List", this);
    m_rescan->setShortcut(QKeySequence(Qt::Key_F5));
    m_properties = new QAction(QIcon(":/icons/settings.svg"), "&Properties...", this);
    m_properties->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    m_openFolder = new QAction(QIcon(":/icons/folder-open.svg"), "Open Game &Folder", this);
    m_exit = new QAction("E&xit", this);

    connect(m_start, &QAction::triggered, this, &MainWindow::play);
    connect(m_addFolder, &QAction::triggered, this, &MainWindow::addFolder);
    connect(m_rescan, &QAction::triggered, this, [this] {
        statusBar()->showMessage("Scanning game folders...");
        m_library.rescan();
    });
    connect(m_properties, &QAction::triggered, this, &MainWindow::showProperties);
    connect(m_openFolder, &QAction::triggered, this, &MainWindow::openGameFolder);
    connect(m_exit, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu("&File");
    file->addAction(m_addFolder);
    file->addAction(m_rescan);
    file->addSeparator();
    file->addAction(m_exit);

    QMenu *game = menuBar()->addMenu("&Game");
    game->addAction(m_start);
    game->addSeparator();
    game->addAction(m_properties);
    game->addAction(m_openFolder);

    QMenu *help = menuBar()->addMenu("&Help");
    help->addAction("&GitHub Repository", this,
                    [] { QDesktopServices::openUrl(QUrl("https://github.com/tonykastaneda/LindLoader")); });
    help->addSeparator();
    help->addAction("&About LindLoader", this, [this] {
        QMessageBox::about(this, "About LindLoader",
                           "<h3>LindLoader</h3>"
                           "<p>Play SEGA Lindbergh arcade games on Windows.</p>"
                           "<p>Built on <a href='https://github.com/lindbergh-loader/linuxloader'>Linux Loader</a> "
                           "by the Linux Loader Development Team.<br>"
                           "Licensed under CC BY-NC-SA 4.0.</p>");
    });
}

void MainWindow::buildToolBar()
{
    QToolBar *bar = addToolBar("Toolbar");
    bar->setMovable(false);
    bar->setIconSize(QSize(24, 24));
    bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    bar->addAction(m_start);
    bar->addSeparator();
    bar->addAction(m_addFolder);
    bar->addAction(m_rescan);
    bar->addSeparator();
    bar->addAction(m_properties);
    bar->addAction(m_openFolder);
    // Shorter labels under the toolbar icons.
    m_start->setIconText("Start");
    m_addFolder->setIconText("Add Folder");
    m_rescan->setIconText("Rescan");
    m_properties->setIconText("Properties");
    m_openFolder->setIconText("Open Folder");
}

QWidget *MainWindow::buildEmptyState()
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->addStretch();
    auto *title = new QLabel("<h2>No games found</h2>");
    title->setAlignment(Qt::AlignCenter);
    auto *text = new QLabel("Add a folder containing your Lindbergh game dumps.\n"
                            "LindLoader scans it and identifies every game inside.");
    text->setAlignment(Qt::AlignCenter);
    auto *btn = new QPushButton(QIcon(":/icons/folder-add.svg"), " Add Game Folder...");
    btn->setMinimumHeight(34);
    connect(btn, &QPushButton::clicked, this, &MainWindow::addFolder);
    l->addWidget(title);
    l->addWidget(text);
    l->addSpacing(12);
    l->addWidget(btn, 0, Qt::AlignHCenter);
    l->addStretch();
    return w;
}

const Game *MainWindow::currentGame() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    if (!item)
        return nullptr;
    const int i = item->data(ColTitle, RoleIndex).toInt();
    return (i >= 0 && i < m_library.games().size()) ? &m_library.games()[i] : nullptr;
}

void MainWindow::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Add Game Folder");
    if (dir.isEmpty())
        return;
    m_library.addFolder(dir);
    statusBar()->showMessage("Scanning game folders...");
    m_library.rescan();
}

void MainWindow::refreshList()
{
    const QString previous = currentGame() ? currentGame()->gameDir : QString();
    m_list->clear();
    const QList<Game> &games = m_library.games();
    for (int i = 0; i < games.size(); ++i)
    {
        const Game &g = games[i];
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(ColTitle, g.title);
        item->setText(ColId, g.gameId);
        item->setText(ColDvp, g.dvp);
        item->setText(ColYear, g.year);
        item->setText(ColResolution, QString(g.resolutions).replace("x", " x "));
        item->setText(ColDump, g.clean ? "Clean" : "Modified");
        item->setForeground(ColDump, g.clean ? QColor("#62c370") : QColor("#e0a040"));
        item->setData(ColTitle, RoleIndex, i);
        if (g.gameDir == previous)
            m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem() && m_list->topLevelItemCount())
        m_list->setCurrentItem(m_list->topLevelItem(0));
    m_stack->setCurrentIndex(games.isEmpty() ? 0 : 1);
    m_statusGames->setText(QStringLiteral("%1 game%2").arg(games.size()).arg(games.size() == 1 ? "" : "s"));
    statusBar()->clearMessage();
    updateActions();
}

void MainWindow::updateActions()
{
    const bool hasGame = currentGame() != nullptr;
    m_start->setEnabled(hasGame && !m_process);
    m_properties->setEnabled(hasGame);
    m_openFolder->setEnabled(hasGame);
}

void MainWindow::showProperties()
{
    if (const Game *g = currentGame())
    {
        GamePropertiesDialog dialog(*g, m_library, this);
        dialog.exec();
    }
}

void MainWindow::openGameFolder()
{
    if (const Game *g = currentGame())
        QDesktopServices::openUrl(QUrl::fromLocalFile(g->gameDir));
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    if (!m_list->itemAt(pos))
        return;
    QMenu menu(this);
    menu.addAction(m_start);
    menu.addSeparator();
    menu.addAction(m_properties);
    menu.addAction(m_openFolder);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void MainWindow::play()
{
    const Game *g = currentGame();
    if (!g || m_process)
        return;
    QString error;
    if (!m_library.ensureConfig(*g, &error))
    {
        QMessageBox::warning(this, "LindLoader", error);
        return;
    }

    // linuxloader.exe is a console program: start it without a console window.
    QString cmd = QStringLiteral("\"%1\" -g \"%2\" -c \"%3\" -o \"%4\"")
                      .arg(QDir::toNativeSeparators(m_library.loaderPath()), QDir::toNativeSeparators(g->gameDir),
                           QDir::toNativeSeparators(g->configPath()), QDir::toNativeSeparators(g->controlsPath()));
    std::wstring wcmd = cmd.toStdWString();
    const std::wstring cwd = QDir::toNativeSeparators(QFileInfo(m_library.loaderPath()).absolutePath()).toStdWString();
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi))
    {
        QMessageBox::warning(this, "LindLoader", "Could not start linuxloader.exe.");
        return;
    }
    CloseHandle(pi.hThread);
    m_process = pi.hProcess;
    m_runningTitle = g->title;
    statusBar()->showMessage("Running " + m_runningTitle);
    m_runTimer->start();
    updateActions();
}

void MainWindow::watchRunning()
{
    if (m_process && WaitForSingleObject(m_process, 0) == WAIT_OBJECT_0)
    {
        CloseHandle(m_process);
        m_process = nullptr;
        m_runTimer->stop();
        statusBar()->showMessage(m_runningTitle + " closed", 5000);
        updateActions();
    }
}
