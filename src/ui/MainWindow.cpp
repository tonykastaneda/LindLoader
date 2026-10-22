#include "MainWindow.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmapCache>
#include <QPushButton>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

#include <windows.h>

#include "ControllerSettingsDialog.h"
#include "GamePropertiesDialog.h"
#include "Gamepads.h"
#include "SettingsDialog.h"
#include "Theme.h"

namespace
{
const int RoleIndex = Qt::UserRole + 1;    // index into GameLibrary::games()
const int RoleSort = Qt::UserRole + 2;     // what the column sorts by
const int RoleType = Qt::UserRole + 3;     // Game::type
const int RoleFavorite = Qt::UserRole + 4;
const int RoleSearch = Qt::UserRole + 5;   // the text the search box matches
const int RoleCover = Qt::UserRole + 6;    // cover image path, empty without one

const int MinTitleWidth = 280;

enum Column
{
    ColTitle,
    ColId,
    ColDvp,
    ColYear,
    ColResolution,
    ColDump,
    ColTimePlayed,
    ColLastPlayed,
    ColSize,
    ColCount
};

// Game::type values and the names the type filter shows for them (the Controls page's layouts).
const struct
{
    const char *type;
    const char *name;
} Types[] = {{"digital", "Standard"}, {"driving", "Racing"}, {"shooting", "Lightgun"}, {"flying", "Flight"}, {"mahjong", "Mahjong"}};

QString formatPlayTime(qint64 seconds)
{
    if (seconds <= 0)
        return {};
    if (seconds < 3600)
    {
        const qint64 minutes = qMax<qint64>(1, seconds / 60);
        return minutes == 1 ? QStringLiteral("1 minute") : QStringLiteral("%1 minutes").arg(minutes);
    }
    const qint64 hours = seconds / 3600;
    return hours == 1 ? QStringLiteral("1 hour") : QStringLiteral("%1 hours").arg(hours);
}
} // namespace

// Filters the game list by the type, favorites and search controls above it.
class GameFilter : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setType(const QString &type)
    {
        m_type = type;
        invalidateFilter();
    }
    void setFavoritesOnly(bool on)
    {
        m_favoritesOnly = on;
        invalidateFilter();
    }
    void setSearch(const QString &text)
    {
        m_search = text.trimmed();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        const QModelIndex i = sourceModel()->index(row, ColTitle, parent);
        if (!m_type.isEmpty() && i.data(RoleType).toString() != m_type)
            return false;
        if (m_favoritesOnly && !i.data(RoleFavorite).toBool())
            return false;
        return m_search.isEmpty() || i.data(RoleSearch).toString().contains(m_search, Qt::CaseInsensitive);
    }

private:
    QString m_type;
    bool m_favoritesOnly = false;
    QString m_search;
};

// Draws a game in the grid like PCSX2: its cover (portrait, 3:4) and optionally its title below it.
// A game without a cover gets a plain card with its title on it.
class CoverDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    int coverWidth = 160;
    bool showTitles = true;

    int coverHeight() const { return coverWidth * 4 / 3; }

    QSize cellSize(const QFontMetrics &fm) const
    {
        return QSize(coverWidth + 2 * Pad, coverHeight() + 2 * Pad + (showTitles ? 2 * fm.height() + 6 : 0));
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        return cellSize(option.fontMetrics);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setRenderHint(QPainter::SmoothPixmapTransform);

        const bool selected = option.state & QStyle::State_Selected;
        const QRect cell = option.rect.adjusted(Pad, Pad, -Pad, -Pad);
        const QRect cover(cell.left() + (cell.width() - coverWidth) / 2, cell.top(), coverWidth, coverHeight());
        const QRect caption(option.rect.left(), cover.bottom() + 1, option.rect.width(),
                            option.rect.bottom() - cover.bottom());
        const QString title = index.data(Qt::DisplayRole).toString();

        // Selected: the caption is highlighted, or the whole cell when titles are hidden.
        if (selected)
        {
            p->setPen(Qt::NoPen);
            p->setBrush(option.palette.highlight());
            p->drawRect(showTitles ? caption : option.rect);
        }
        const QPixmap pix = loadCover(index.data(RoleCover).toString(), cover.size());
        if (!pix.isNull())
        {
            QRect r = pix.rect();
            r.moveCenter(cover.center());
            p->drawPixmap(r, pix);
        }
        else
        {
            QLinearGradient fill(cover.topLeft(), cover.bottomRight());
            fill.setColorAt(0, QColor("#3a3a3a"));
            fill.setColorAt(1, QColor("#242424"));
            p->setPen(Qt::NoPen);
            p->setBrush(fill);
            p->drawRect(cover);
            QFont font = p->font();
            font.setPointSizeF(font.pointSizeF() * qBound(1.0, coverWidth / 130.0, 2.0));
            p->setFont(font);
            p->setPen(QColor("#b4b4b4"));
            p->drawText(cover.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap, title);
            p->setFont(option.font);
        }

        if (showTitles)
        {
            const QRect text = caption.adjusted(Pad, 6, -Pad, 0);
            p->setClipRect(text);
            p->setPen(selected ? option.palette.highlightedText().color() : option.palette.text().color());
            p->drawText(text, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, title);
        }
        p->restore();
    }

private:
    static constexpr int Pad = 6;

    static QPixmap loadCover(const QString &path, const QSize &size)
    {
        if (path.isEmpty())
            return {};
        const QString key = QStringLiteral("cover:%1:%2").arg(path).arg(size.width());
        QPixmap pix;
        if (!QPixmapCache::find(key, &pix))
        {
            const QImage image(path);
            if (!image.isNull())
                pix = QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            QPixmapCache::insert(key, pix);
        }
        return pix;
    }
};

MainWindow::MainWindow(const QString &appDir, QWidget *parent) : QMainWindow(parent), m_appDir(appDir), m_library(appDir)
{
    setWindowTitle("Lindy");
    resize(1280, 720);
    Theme::apply(m_library.setting("interface/theme").toString());
    // Lindy reads controllers for binding with the same SDL input sources the games use.
    for (const QString &var : m_library.launchEnvironment())
        if (var.startsWith("SDL_JOYSTICK_") || var.startsWith("SDL_XINPUT_"))
            qputenv(var.section('=', 0, 0).toUtf8(), var.section('=', 1).toUtf8());

    buildActions();
    buildMenus();
    buildToolBar();

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(
        {"Title", "Game ID", "DVP", "Year", "Native Resolution", "Dump", "Time Played", "Last Played", "Size"});
    m_filter = new GameFilter(this);
    m_filter->setSourceModel(m_model);
    m_filter->setSortRole(RoleSort);
    m_filter->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_list = new QTreeView;
    m_list->setModel(m_filter);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setSortingEnabled(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->header()->setStretchLastSection(false);
    m_list->setColumnWidth(ColId, 90);
    m_list->setColumnWidth(ColDvp, 100);
    m_list->setColumnWidth(ColYear, 60);
    m_list->setColumnWidth(ColResolution, 150);
    m_list->setColumnWidth(ColDump, 90);
    m_list->setColumnWidth(ColTimePlayed, 110);
    m_list->setColumnWidth(ColLastPlayed, 110);
    m_list->setColumnWidth(ColSize, 100);
    m_list->sortByColumn(ColTitle, Qt::AscendingOrder);

    // The grid shows the same rows and shares the list's selection.
    m_coverDelegate = new CoverDelegate(this);
    m_grid = new QListView;
    m_grid->setModel(m_filter);
    QItemSelectionModel *gridSelection = m_grid->selectionModel();
    m_grid->setSelectionModel(m_list->selectionModel());
    delete gridSelection;
    m_grid->setModelColumn(ColTitle);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setWrapping(true);
    m_grid->setSpacing(6);
    m_grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
    m_grid->setItemDelegate(m_coverDelegate);

    m_views = new QStackedWidget;
    m_views->addWidget(m_list);
    m_views->addWidget(m_grid);

    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(buildListBar());
    pageLayout->addWidget(m_views);

    m_stack = new QStackedWidget;
    m_stack->addWidget(buildEmptyState());
    m_stack->addWidget(page);
    setCentralWidget(m_stack);

    m_statusGames = new QLabel;
    statusBar()->addPermanentWidget(m_statusGames);

    connect(m_list->header(), &QHeaderView::sectionResized, this, [this](int column) {
        if (column != ColTitle)
            fitTitleColumn();
    });
    for (QAbstractItemView *view : {static_cast<QAbstractItemView *>(m_list), static_cast<QAbstractItemView *>(m_grid)})
    {
        view->viewport()->installEventFilter(this);
        connect(view, &QAbstractItemView::doubleClicked, this, &MainWindow::play);
        connect(view, &QWidget::customContextMenuRequested, this,
                [this, view](const QPoint &pos) { showContextMenu(view, pos); });
    }
    connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateActions);
    connect(&m_library, &GameLibrary::scanned, this, &MainWindow::refreshList);
    connect(&m_library, &GameLibrary::extracting, this, [this](const QString &image, int percent) {
        statusBar()->showMessage(QStringLiteral("Unpacking %1... %2%").arg(image).arg(percent));
    });
    connect(&m_library, &GameLibrary::extractFailed, this, [this](const QString &image, const QString &error) {
        QMessageBox::warning(this, "Lindy", QStringLiteral("Could not unpack %1: %2").arg(image, error));
    });

    m_runTimer = new QTimer(this);
    m_runTimer->setInterval(1000);
    connect(m_runTimer, &QTimer::timeout, this, &MainWindow::watchRunning);
    m_arrangeTimer = new QTimer(this);
    m_arrangeTimer->setInterval(500);
    connect(m_arrangeTimer, &QTimer::timeout, this, &MainWindow::arrangeWindows);

    applyInterfaceSettings();
    if (!QFileInfo::exists(m_library.loaderPath()))
        QMessageBox::warning(this, "Lindy", "linuxloader.exe was not found next to Lindy.exe.");
    updateActions();
    statusBar()->showMessage("Scanning game folders...");
    m_library.rescan();
}

void MainWindow::buildActions()
{
    m_start = new QAction(Theme::icon("play"), "&Start Game", this);
    m_start->setShortcut(QKeySequence(Qt::Key_Return));
    m_addFolder = new QAction(Theme::icon("folder-add"), "&Add Game Folder...", this);
    m_rescan = new QAction(Theme::icon("refresh"), "&Rescan Game List", this);
    m_rescan->setShortcut(QKeySequence(Qt::Key_F5));
    m_properties = new QAction(Theme::icon("settings"), "&Properties...", this);
    m_properties->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    m_controls = new QAction(Theme::icon("gamepad"), "Game &Controls...", this);
    m_openFolder = new QAction(Theme::icon("folder-open"), "Open Game &Folder", this);
    m_favorite = new QAction(Theme::icon("star"), "Add to F&avorites", this);
    m_linkedPlay = new QAction(Theme::icon("play"), "Start &Linked Play...", this);
    m_settings = new QAction(Theme::icon("sliders"), "&Settings...", this);
    m_controllerSettings = new QAction(Theme::icon("keyboard"), "&Controller Settings...", this);
    m_exit = new QAction("E&xit", this);

    connect(m_start, &QAction::triggered, this, &MainWindow::play);
    connect(m_addFolder, &QAction::triggered, this, &MainWindow::addFolder);
    connect(m_rescan, &QAction::triggered, this, [this] {
        statusBar()->showMessage("Scanning game folders...");
        m_library.rescan();
    });
    connect(m_properties, &QAction::triggered, this, [this] { showProperties(GamePropertiesDialog::Summary); });
    connect(m_controls, &QAction::triggered, this, [this] { showProperties(GamePropertiesDialog::Controls); });
    connect(m_openFolder, &QAction::triggered, this, &MainWindow::openGameFolder);
    connect(m_favorite, &QAction::triggered, this, &MainWindow::toggleFavorite);
    connect(m_linkedPlay, &QAction::triggered, this, &MainWindow::startLinkedPlay);
    connect(m_settings, &QAction::triggered, this, [this] { showSettings(SettingsDialog::Interface); });
    connect(m_controllerSettings, &QAction::triggered, this,
            [this] { showControllerSettings(ControllerSettingsDialog::GlobalSettings); });
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
    game->addAction(m_linkedPlay);
    game->addSeparator();
    game->addAction(m_properties);
    game->addAction(m_controls);
    game->addAction(m_openFolder);
    game->addSeparator();
    game->addAction(m_favorite);

    // Like PCSX2's Settings menu: one entry per page.
    QMenu *settings = menuBar()->addMenu("&Settings");
    const QString pages[] = {"&Interface...", "&Game List...", "&Audio...", "&Network...", "&Folders..."};
    for (int i = 0; i < 5; ++i)
        settings->addAction(pages[i], this, [this, i] { showSettings(i); });
    settings->addSeparator();
    settings->addAction(m_controllerSettings);
    settings->addAction("&Hotkeys...", this, [this] { showControllerSettings(ControllerSettingsDialog::Hotkeys); });

    QMenu *help = menuBar()->addMenu("&Help");
    help->addAction("&GitHub Repository", this,
                    [] { QDesktopServices::openUrl(QUrl("https://github.com/tonykastaneda/Lindy")); });
    help->addSeparator();
    help->addAction("&About Lindy", this, [this] {
        QMessageBox::about(this, "About Lindy",
                           "<h3>Lindy</h3>"
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
    bar->addAction(m_controls);
    bar->addAction(m_openFolder);
    bar->addSeparator();
    bar->addAction(m_settings);
    bar->addAction(m_controllerSettings);
    // Shorter labels under the toolbar icons.
    m_start->setIconText("Start");
    m_addFolder->setIconText("Add Folder");
    m_rescan->setIconText("Rescan");
    m_properties->setIconText("Properties");
    m_controls->setIconText("Controls");
    m_openFolder->setIconText("Open Folder");
    m_settings->setIconText("Settings");
    m_controllerSettings->setIconText("Controllers");
}

// The bar above the games, like PCSX2's: list or grid, grid titles and cover size on the left;
// favorites, type and search filters on the right.
QWidget *MainWindow::buildListBar()
{
    auto *bar = new QWidget;
    bar->setObjectName("listBar");
    bar->setStyleSheet("#listBar { background: palette(base); border-bottom: 1px solid palette(mid); }");
    auto *l = new QHBoxLayout(bar);
    l->setContentsMargins(8, 4, 8, 4);
    l->setSpacing(4);

    auto button = [bar](const QString &icon, const QString &tip) {
        auto *b = new QToolButton(bar);
        b->setIcon(Theme::icon(icon));
        b->setIconSize(QSize(20, 20));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setCheckable(true);
        return b;
    };
    m_listButton = button("list", "Game List");
    m_gridButton = button("grid", "Game Grid");
    m_titlesButton = button("tag", "Show Titles in the Grid");
    auto *viewGroup = new QButtonGroup(bar);
    viewGroup->addButton(m_listButton);
    viewGroup->addButton(m_gridButton);

    m_zoom = new QSlider(Qt::Horizontal, bar);
    m_zoom->setRange(100, 320);
    m_zoom->setFixedWidth(200);
    m_zoom->setToolTip("Cover Size");

    m_favoritesButton = button("star", "Show Favorites Only");
    m_favoritesButton->setIcon(Theme::icon("star", "star-filled"));

    m_typeFilter = new QComboBox(bar);
    m_typeFilter->addItem(Theme::icon("filter"), "All Types", QString());
    for (const auto &t : Types)
        m_typeFilter->addItem(Theme::icon("filter"), t.name, QString(t.type));
    m_typeFilter->setMinimumWidth(150);

    m_search = new QLineEdit(bar);
    m_search->setPlaceholderText("Search...");
    m_search->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(260);

    l->addWidget(m_listButton);
    l->addWidget(m_gridButton);
    l->addSpacing(6);
    l->addWidget(m_titlesButton);
    l->addSpacing(6);
    l->addWidget(m_zoom);
    l->addStretch();
    l->addWidget(m_favoritesButton);
    l->addSpacing(4);
    l->addWidget(m_typeFilter);
    l->addSpacing(4);
    l->addWidget(m_search);

    // Restore the last view and filters.
    m_titlesButton->setChecked(m_library.setting("ui/gridTitles", true).toBool());
    m_zoom->setValue(m_library.setting("ui/coverWidth", 160).toInt());
    m_favoritesButton->setChecked(m_library.setting("ui/favoritesOnly", false).toBool());
    m_typeFilter->setCurrentIndex(qMax(0, m_typeFilter->findData(m_library.setting("ui/type").toString())));
    m_filter->setFavoritesOnly(m_favoritesButton->isChecked());
    m_filter->setType(m_typeFilter->currentData().toString());
    updateGridSize();
    setGridView(m_library.setting("ui/view").toString() == "grid");

    connect(m_listButton, &QToolButton::clicked, this, [this] { setGridView(false); });
    connect(m_gridButton, &QToolButton::clicked, this, [this] { setGridView(true); });
    connect(m_titlesButton, &QToolButton::toggled, this, [this](bool on) {
        m_library.setSetting("ui/gridTitles", on);
        updateGridSize();
    });
    connect(m_zoom, &QSlider::valueChanged, this, [this](int value) {
        m_library.setSetting("ui/coverWidth", value);
        updateGridSize();
    });
    connect(m_favoritesButton, &QToolButton::toggled, this, [this](bool on) {
        m_library.setSetting("ui/favoritesOnly", on);
        m_filter->setFavoritesOnly(on);
        refreshList();
    });
    connect(m_typeFilter, &QComboBox::currentIndexChanged, this, [this] {
        m_library.setSetting("ui/type", m_typeFilter->currentData());
        m_filter->setType(m_typeFilter->currentData().toString());
        refreshList();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filter->setSearch(text);
        refreshList();
    });
    return bar;
}

QWidget *MainWindow::buildEmptyState()
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->addStretch();
    auto *title = new QLabel("<h2>No games found</h2>");
    title->setAlignment(Qt::AlignCenter);
    auto *text = new QLabel("Add a folder containing your Lindbergh game dumps.\n"
                            "Lindy scans it and identifies every game inside.");
    text->setAlignment(Qt::AlignCenter);
    auto *btn = new QPushButton(Theme::icon("folder-add"), " Add Game Folder...");
    btn->setMinimumHeight(34);
    connect(btn, &QPushButton::clicked, this, &MainWindow::addFolder);
    l->addWidget(title);
    l->addWidget(text);
    l->addSpacing(12);
    l->addWidget(btn, 0, Qt::AlignHCenter);
    l->addStretch();
    return w;
}

void MainWindow::setGridView(bool grid)
{
    m_views->setCurrentIndex(grid ? 1 : 0);
    m_listButton->setChecked(!grid);
    m_gridButton->setChecked(grid);
    m_titlesButton->setEnabled(grid);
    m_zoom->setEnabled(grid);
    m_library.setSetting("ui/view", grid ? "grid" : "list");
}

void MainWindow::updateGridSize()
{
    m_coverDelegate->coverWidth = m_zoom->value();
    m_coverDelegate->showTitles = m_titlesButton->isChecked();
    m_grid->setGridSize(m_coverDelegate->cellSize(m_grid->fontMetrics()) + QSize(6, 6));
    m_grid->viewport()->update();
}

// The title takes the room the other columns leave, but stays readable: on a narrow window the
// list scrolls sideways instead.
void MainWindow::fitTitleColumn()
{
    int others = 0;
    for (int c = ColTitle + 1; c < ColCount; ++c)
        others += m_list->columnWidth(c);
    m_list->setColumnWidth(ColTitle, qMax(MinTitleWidth, m_list->viewport()->width() - others));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_list->viewport() && event->type() == QEvent::Resize)
        fitTitleColumn();
    // The background goes under the games: the view paints them after this.
    if ((watched == m_list->viewport() || watched == m_grid->viewport()) && event->type() == QEvent::Paint &&
        !m_background.isNull())
        paintBackground(static_cast<QWidget *>(watched));

    // Clicking the empty area around the games deselects, like Explorer.
    if ((watched == m_list->viewport() || watched == m_grid->viewport()) && event->type() == QEvent::MouseButtonPress)
    {
        QAbstractItemView *view = watched == m_list->viewport() ? static_cast<QAbstractItemView *>(m_list) : m_grid;
        if (!view->indexAt(static_cast<QMouseEvent *>(event)->position().toPoint()).isValid())
            m_list->selectionModel()->clear();
    }
    return QMainWindow::eventFilter(watched, event);
}

const Game *MainWindow::currentGame() const
{
    // The highlighted row, not the current item: selecting a row through accessibility tools doesn't
    // move the current item, and the list sets a current item on focus without selecting it.
    const QModelIndexList selected = m_list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
        return nullptr;
    const int i = selected.first().siblingAtColumn(ColTitle).data(RoleIndex).toInt();
    return (i >= 0 && i < m_library.games().size()) ? &m_library.games()[i] : nullptr;
}

// <covers folder>\<short title, game ID or title>.png, .jpg or .webp, like PCSX2's covers folder.
QString MainWindow::coverPath(const Game &game) const
{
    for (const QString &name : {game.shortTitle, game.gameId, game.title})
        for (const char *ext : {"png", "jpg", "jpeg", "webp"})
        {
            const QString path = QStringLiteral("%1/%2.%3").arg(m_library.coversDir(), name, ext);
            if (!name.isEmpty() && QFileInfo::exists(path))
                return path;
        }
    return {};
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
    m_model->removeRows(0, m_model->rowCount());
    const QList<Game> &games = m_library.games();
    for (int i = 0; i < games.size(); ++i)
    {
        const Game &g = games[i];
        auto cell = [](const QString &text, const QVariant &sort) {
            auto *item = new QStandardItem(text);
            item->setEditable(false);
            item->setData(sort, RoleSort);
            return item;
        };
        const bool favorite = m_library.isFavorite(g);
        const qint64 played = m_library.playSeconds(g);
        const QDateTime last = m_library.lastPlayed(g);

        QStandardItem *title = cell(g.title, g.title);
        title->setData(i, RoleIndex);
        title->setData(g.type, RoleType);
        title->setData(favorite, RoleFavorite);
        title->setData(QStringList{g.title, g.gameId, g.dvp, g.shortTitle}.join('\n'), RoleSearch);
        title->setData(coverPath(g), RoleCover);
        if (favorite)
            title->setIcon(Theme::icon("star-filled"));
        QStandardItem *dump = cell(g.clean ? "Clean" : "Modified", g.clean);
        dump->setForeground(g.clean ? QColor("#62c370") : QColor("#e0a040"));

        m_model->appendRow({title, cell(g.gameId, g.gameId), cell(g.dvp, g.dvp), cell(g.year, g.year),
                            cell(QString(g.resolutions).replace("x", " x "), g.resolutions), dump,
                            cell(formatPlayTime(played), played),
                            cell(last.isValid() ? QLocale().toString(last.date(), QLocale::ShortFormat) : "Never",
                                 last.isValid() ? last.toMSecsSinceEpoch() : 0),
                            cell(QStringLiteral("%1 MB").arg(g.size / 1048576.0, 0, 'f', 2), g.size)});
        if (g.gameDir == previous)
        {
            const QModelIndex index = m_filter->mapFromSource(m_model->index(i, ColTitle));
            if (index.isValid())
                m_list->selectionModel()->setCurrentIndex(
                    index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
    m_stack->setCurrentIndex(games.isEmpty() ? 0 : 1);
    const int shown = m_filter->rowCount();
    m_statusGames->setText(shown == games.size()
                               ? QStringLiteral("%1 game%2").arg(games.size()).arg(games.size() == 1 ? "" : "s")
                               : QStringLiteral("%1 of %2 games").arg(shown).arg(games.size()));
    statusBar()->clearMessage();
    updateActions();
}

void MainWindow::updateActions()
{
    const Game *g = currentGame();
    const bool hasGame = g != nullptr;
    m_start->setEnabled(hasGame && m_processes.isEmpty());
    m_linkedPlay->setEnabled(hasGame && m_processes.isEmpty() && GameLibrary::linkCabinets(*g) > 0);
    m_properties->setEnabled(hasGame);
    m_controls->setEnabled(hasGame);
    m_openFolder->setEnabled(hasGame);
    m_favorite->setEnabled(hasGame);
    m_favorite->setText(g && m_library.isFavorite(*g) ? "Remove from F&avorites" : "Add to F&avorites");

    // The selected game's folder in the status bar, like PCSX2; a running game's message stays.
    if (m_processes.isEmpty())
    {
        if (g)
            statusBar()->showMessage(QDir::toNativeSeparators(g->gameDir));
        else
            statusBar()->clearMessage();
    }
}

void MainWindow::showProperties(int page)
{
    // A separate window like DuckStation's and PCSX2's: the game list stays usable.
    if (const Game *g = currentGame())
    {
        auto *dialog = new GamePropertiesDialog(*g, m_library, static_cast<GamePropertiesDialog::Page>(page), this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
}

void MainWindow::openGameFolder()
{
    if (const Game *g = currentGame())
        QDesktopServices::openUrl(QUrl::fromLocalFile(g->gameDir));
}

void MainWindow::toggleFavorite()
{
    if (const Game *g = currentGame())
    {
        m_library.setFavorite(*g, !m_library.isFavorite(*g));
        refreshList();
    }
}

void MainWindow::showContextMenu(QAbstractItemView *view, const QPoint &pos)
{
    if (!view->indexAt(pos).isValid())
        return;
    QMenu menu(this);
    menu.addAction(m_start);
    if (m_linkedPlay->isEnabled())
        menu.addAction(m_linkedPlay);
    menu.addSeparator();
    menu.addAction(m_properties);
    menu.addAction(m_controls);
    menu.addAction(m_openFolder);
    menu.addSeparator();
    menu.addAction(m_favorite);
    menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::play()
{
    const Game *g = currentGame();
    if (!g || !m_processes.isEmpty())
        return;
    QString error;
    if (!m_library.applySettings(*g, &error))
    {
        QMessageBox::warning(this, "Lindy", error);
        return;
    }
    void *process = launch(*g, g->configDir, {});
    if (!process)
        return;
    started(*g, {process});
}

// Starts the loader for game with the settings in configDir; returns its process HANDLE.
void *MainWindow::launch(const Game &game, const QString &configDir, const QStringList &extraEnvironment)
{
    // Lindy's input, audio and screensaver settings reach the loader's SDL as environment variables.
    std::wstring environment;
    for (const QString &var : m_library.launchEnvironment() + extraEnvironment)
        environment += var.toStdWString() + L'\0';
    environment += L'\0';

    // linuxloader.exe is a console program: start it without a console window.
    const QString config = configDir + "/linuxloader.ini", controls = configDir + "/controls.ini";
    QString cmd = QStringLiteral("\"%1\" -g \"%2\" -c \"%3\" -o \"%4\"")
                      .arg(QDir::toNativeSeparators(m_library.loaderPath()), QDir::toNativeSeparators(game.gameDir),
                           QDir::toNativeSeparators(config), QDir::toNativeSeparators(controls));
    std::wstring wcmd = cmd.toStdWString();
    const std::wstring cwd = QDir::toNativeSeparators(QFileInfo(m_library.loaderPath()).absolutePath()).toStdWString();
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        environment.data(), cwd.c_str(), &si, &pi))
    {
        QMessageBox::warning(this, "Lindy", "Could not start linuxloader.exe.");
        return nullptr;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

void MainWindow::started(const Game &game, const QList<void *> &processes)
{
    m_processes = processes;
    m_runningGame = game;
    m_runningSince = QDateTime::currentDateTime();
    statusBar()->showMessage(processes.size() > 1
                                 ? QStringLiteral("Running %1 (%2 cabinets)").arg(game.title).arg(processes.size())
                                 : "Running " + game.title);
    m_runTimer->start();
    updateActions();
    if (m_library.setting("interface/hideMainWindow").toBool())
        hide();
}

// Linked play on this PC: one loader per cabinet, each with its own controller, side by side.
void MainWindow::startLinkedPlay()
{
    const Game *g = currentGame();
    if (!g || !m_processes.isEmpty() || GameLibrary::linkCabinets(*g) == 0)
        return;
    const Game game = *g;

    // Not modal-blocking (open, not exec): the game list stays as it is behind it.
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Linked Play");
    auto *intro = new QLabel(QStringLiteral(
        "<b>%1</b> runs once per cabinet, linked as on an arcade floor. Each cabinet has its own window and "
        "controller.<br><br>Pick <b>Versus</b> (対戦プレイ) in every cabinet to race each other.").arg(game.title));
    intro->setWordWrap(true);
    auto *count = new QSpinBox;
    count->setRange(2, GameLibrary::linkCabinets(game));
    count->setValue(2);
    auto *pads = new QFormLayout;
    auto combos = std::make_shared<QList<QComboBox *>>();
    auto fillPads = [count, pads, combos] {
        while (pads->rowCount() > 0)
            pads->removeRow(0);
        combos->clear();
        for (int c = 1; c <= count->value(); ++c)
        {
            auto *combo = new QComboBox;
            combo->addItem(QStringLiteral("Automatic (controller %1)").arg(c), QString());
            combo->addItem("Keyboard", GameLibrary::KeyboardController);
            for (const Gamepads::Device &d : Gamepads::instance().devices())
                combo->addItem(d.name, d.guid);
            *combos << combo;
            pads->addRow(QStringLiteral("Cabinet %1%2:").arg(c).arg(c == 1 ? " (Main)" : ""), combo);
        }
    };
    fillPads();
    connect(count, &QSpinBox::valueChanged, dialog, fillPads);
    auto *sound = new QCheckBox("Sound from cabinet 1 only");
    sound->setChecked(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Start");
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto *form = new QFormLayout;
    form->addRow("Cabinets:", count);
    auto *padsBox = new QGroupBox("Controllers");
    padsBox->setLayout(pads);
    auto *l = new QVBoxLayout(dialog);
    l->addWidget(intro);
    l->addLayout(form);
    l->addWidget(padsBox);
    l->addWidget(sound);
    l->addWidget(buttons);
    connect(dialog, &QDialog::accepted, this, [this, game, combos, sound] {
        QStringList guids;
        for (QComboBox *c : *combos)
            guids << c->currentData().toString();
        startLinkedCabinets(game, guids, sound->isChecked());
    });
    dialog->open();
}

void MainWindow::startLinkedCabinets(const Game &game, const QStringList &controllerGuids, bool soundFromFirstOnly)
{
    if (!m_processes.isEmpty())
        return;
    QString error;
    if (!m_library.applySettings(game, &error))
    {
        QMessageBox::warning(this, "Lindy", error);
        return;
    }
    QStringList dirs;
    for (int c = 1; c <= controllerGuids.size(); ++c)
    {
        const QString dir =
            m_library.prepareLinkCabinet(game, c, controllerGuids[c - 1], c == 1 || !soundFromFirstOnly, &error);
        if (dir.isEmpty())
        {
            QMessageBox::warning(this, "Lindy", error);
            return;
        }
        dirs << dir;
    }
    // The keyboard only reaches the window with the focus: give it to the keyboard cabinet's.
    m_keyboardCabinet = controllerGuids.indexOf(GameLibrary::KeyboardController);
    // Every cabinet reads its controller while another cabinet's window has the focus.
    QList<void *> processes;
    for (const QString &dir : dirs)
        if (void *p = launch(game, dir, {"SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1"}))
            processes << p;
    if (processes.isEmpty())
        return;
    started(game, processes);
    m_arranged.clear();
    m_arrangeTries = 0;
    m_arrangeTimer->start();
}

// Tiles the cabinets' windows on the screen Lindy is on, as they appear: two side by side, three or
// four in a grid. A game sets its window's size once more after showing it, so each window is put
// in place again for a few seconds.
void MainWindow::arrangeWindows()
{
    if (++m_arrangeTries > 120 || m_processes.size() < 2)
    {
        m_arrangeTimer->stop();
        return;
    }
    // The whole monitor Lindy is on, in physical pixels: borderless tiles cover it like one split screen.
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(reinterpret_cast<HWND>(winId()), MONITOR_DEFAULTTOPRIMARY), &mi);
    const QRect area(mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top);
    const int n = m_processes.size(), cols = 2, rows = n == 2 ? 1 : 2;
    const int settleTicks = 30; // 15 s at 500 ms
    for (int i = 0; i < n; ++i)
    {
        if (m_arranged.contains(i) && m_arrangeTries - m_arranged[i] > settleTicks)
            continue;
        struct Find
        {
            DWORD pid;
            HWND hwnd;
        } find{GetProcessId(m_processes[i]), nullptr};
        EnumWindows(
            [](HWND h, LPARAM p) -> BOOL {
                auto *f = reinterpret_cast<Find *>(p);
                DWORD pid = 0;
                GetWindowThreadProcessId(h, &pid);
                if (pid == f->pid && IsWindowVisible(h) && GetWindowTextLengthW(h) > 0)
                {
                    f->hwnd = h;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&find));
        if (!find.hwnd)
            continue;
        const int w = area.width() / cols, h = area.height() / rows;
        const QRect cell(area.x() + (i % cols) * w, area.y() + (i / cols) * h, w, h);
        // No title bar or border. With a keyboard cabinet, a click on another cabinet doesn't take the focus
        // (the keyboard only reaches the focused window; controllers reach every cabinet anyway).
        const LONG_PTR style = GetWindowLongPtrW(find.hwnd, GWL_STYLE);
        const LONG_PTR exStyle = GetWindowLongPtrW(find.hwnd, GWL_EXSTYLE);
        LONG_PTR newStyle = (style & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP;
        LONG_PTR newExStyle = exStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE | WS_EX_STATICEDGE);
        if (m_keyboardCabinet >= 0 && i != m_keyboardCabinet)
            newExStyle |= WS_EX_NOACTIVATE;
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
        if (newStyle != style || newExStyle != exStyle)
        {
            SetWindowLongPtrW(find.hwnd, GWL_STYLE, newStyle);
            SetWindowLongPtrW(find.hwnd, GWL_EXSTYLE, newExStyle);
            flags |= SWP_FRAMECHANGED;
        }
        SetWindowPos(find.hwnd, nullptr, cell.x(), cell.y(), cell.width(), cell.height(), flags);
        if (i == m_keyboardCabinet && GetForegroundWindow() != find.hwnd)
            SetForegroundWindow(find.hwnd);
        if (!m_arranged.contains(i))
            m_arranged.insert(i, m_arrangeTries);
    }
    bool settled = m_arranged.size() == n;
    for (int tick : m_arranged)
        settled &= m_arrangeTries - tick > settleTicks;
    if (settled)
        m_arrangeTimer->stop();
}

void MainWindow::watchRunning()
{
    for (int i = m_processes.size() - 1; i >= 0; --i)
        if (WaitForSingleObject(m_processes[i], 0) == WAIT_OBJECT_0)
        {
            CloseHandle(m_processes[i]);
            m_processes.removeAt(i);
            m_arranged.clear();
        }
    if (m_processes.isEmpty() && m_runTimer->isActive())
    {
        m_runTimer->stop();
        m_library.recordPlay(m_runningGame, m_runningSince.secsTo(QDateTime::currentDateTime()));
        refreshList();
        if (isHidden())
            show();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Closing Lindy closes the running game too, after asking.
    if (!m_processes.isEmpty())
    {
        if (m_library.setting("interface/confirmExit").toBool() &&
            QMessageBox::question(this, "Lindy", m_runningGame.title + " is running. Close it and exit?") != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
        for (void *p : m_processes)
        {
            TerminateProcess(p, 0);
            CloseHandle(p);
        }
        m_processes.clear();
        m_library.recordPlay(m_runningGame, m_runningSince.secsTo(QDateTime::currentDateTime()));
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::showSettings(int page)
{
    auto *dialog = new SettingsDialog(m_library, static_cast<SettingsDialog::Page>(page), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &SettingsWindow::settingsChanged, this, &MainWindow::applyInterfaceSettings);
    dialog->show();
}

void MainWindow::showControllerSettings(int page)
{
    auto *dialog = new ControllerSettingsDialog(m_library, static_cast<ControllerSettingsDialog::Page>(page), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

// Settings > Interface and Folders that show in this window: the background and the covers.
void MainWindow::applyInterfaceSettings()
{
    const QString file = m_library.setting("interface/background").toString();
    m_background = file.isEmpty() ? QPixmap() : QPixmap(file);
    // With a background the alternate rows let it show through.
    QPalette p = palette();
    if (!m_background.isNull())
    {
        QColor alternate = p.color(QPalette::AlternateBase);
        alternate.setAlpha(110);
        p.setColor(QPalette::AlternateBase, alternate);
    }
    m_list->setPalette(p);
    m_list->viewport()->update();
    m_grid->viewport()->update();

    // The covers folder may have changed.
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        QStandardItem *title = m_model->item(row, ColTitle);
        const int i = title->data(RoleIndex).toInt();
        if (i >= 0 && i < m_library.games().size())
            title->setData(coverPath(m_library.games()[i]), RoleCover);
    }
}

void MainWindow::paintBackground(QWidget *viewport)
{
    QPainter p(viewport);
    p.fillRect(viewport->rect(), viewport->palette().color(QPalette::Base));
    p.setOpacity(m_library.setting("interface/backgroundOpacity").toInt() / 100.0);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QString scaling = m_library.setting("interface/backgroundScaling").toString();
    const QRect area = viewport->rect();
    if (scaling == "tile")
    {
        p.drawTiledPixmap(area, m_background);
        return;
    }
    QSize size = m_background.size();
    if (scaling == "fill")
        size.scale(area.size(), Qt::KeepAspectRatioByExpanding);
    else if (scaling == "fit")
        size.scale(area.size(), Qt::KeepAspectRatio);
    else if (scaling == "stretch")
        size = area.size();
    QRect target(QPoint(0, 0), size);
    target.moveCenter(area.center());
    p.drawPixmap(target, m_background);
}
