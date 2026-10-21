#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <windows.h>

namespace
{
const int RoleIndex = Qt::UserRole + 1;

// Game list rows: title, IDs underneath, a dump badge on the right.
class GameDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {0, 64}; }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRect r = opt.rect.adjusted(8, 3, -8, -3);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hover = opt.state & QStyle::State_MouseOver;
        if (selected || hover)
        {
            p->setPen(Qt::NoPen);
            p->setBrush(selected ? QColor("#2a2f3a") : QColor("#22262f"));
            p->drawRoundedRect(r, 8, 8);
        }
        if (selected)
        {
            p->setBrush(QColor("#f5b800"));
            p->drawRoundedRect(QRect(r.left(), r.top() + 14, 3, r.height() - 28), 1.5, 1.5);
        }

        const QString title = index.data(Qt::DisplayRole).toString();
        const QString sub = index.data(Qt::UserRole).toString();
        const bool clean = index.data(Qt::UserRole + 2).toBool();

        const QString badge = clean ? "CLEAN" : "MODIFIED";
        QFont bf = opt.font;
        bf.setPointSizeF(7.5);
        bf.setBold(true);
        bf.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        const int bw = QFontMetrics(bf).horizontalAdvance(badge) + 14;
        const QRect br(r.right() - bw - 12, r.center().y() - 9, bw, 18);
        p->setPen(Qt::NoPen);
        p->setBrush(clean ? QColor(46, 160, 98, 40) : QColor(230, 150, 40, 45));
        p->drawRoundedRect(br, 9, 9);
        p->setFont(bf);
        p->setPen(clean ? QColor("#4fd18b") : QColor("#f0a43a"));
        p->drawText(br, Qt::AlignCenter, badge);

        QFont tf = opt.font;
        tf.setPointSizeF(10.5);
        tf.setWeight(QFont::DemiBold);
        p->setFont(tf);
        p->setPen(QColor("#eceef3"));
        const QRect textRect(r.left() + 16, r.top() + 11, br.left() - r.left() - 28, 22);
        p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(tf).elidedText(title, Qt::ElideRight, textRect.width()));

        QFont sf = opt.font;
        sf.setPointSizeF(8.5);
        p->setFont(sf);
        p->setPen(QColor("#8b93a1"));
        p->drawText(textRect.translated(0, 21), Qt::AlignLeft | Qt::AlignVCenter, sub);
        p->restore();
    }
};

struct Resolution
{
    const char *label;
    const char *width;
    const char *height;
};

const Resolution kResolutions[] = {
    {"Native", "AUTO", "AUTO"},
    {"1280 × 720  ·  16:9", "1280", "720"},
    {"1920 × 1080  ·  16:9", "1920", "1080"},
    {"2560 × 1440  ·  16:9", "2560", "1440"},
    {"3840 × 2160  ·  16:9", "3840", "2160"},
    {"640 × 480  ·  4:3", "640", "480"},
    {"1440 × 1080  ·  4:3", "1440", "1080"},
    {"2880 × 2160  ·  4:3", "2880", "2160"},
};
} // namespace

MainWindow::MainWindow(const QString &appDir, QWidget *parent) : QMainWindow(parent), m_library(appDir)
{
    setWindowTitle("LindLoader");
    resize(1180, 740);
    setMinimumSize(900, 560);

    auto *central = new QWidget;
    central->setObjectName("central");
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top bar
    auto *top = new QWidget;
    top->setObjectName("topBar");
    auto *topL = new QHBoxLayout(top);
    topL->setContentsMargins(22, 14, 18, 14);
    auto *mark = new QLabel("<span style='color:#f5b800'>●</span>&nbsp; LindLoader");
    mark->setObjectName("wordmark");
    topL->addWidget(mark);
    topL->addStretch();
    auto *rescan = new QPushButton("Rescan");
    rescan->setObjectName("ghost");
    auto *add = new QPushButton("Add Folder");
    add->setObjectName("secondary");
    topL->addWidget(rescan);
    topL->addWidget(add);
    root->addWidget(top);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(buildLibraryPanel());
    m_stack = new QStackedWidget;
    m_stack->addWidget(buildEmptyState());
    m_stack->addWidget(buildDetailPanel());
    body->addWidget(m_stack, 1);
    root->addLayout(body, 1);
    setCentralWidget(central);

    connect(add, &QPushButton::clicked, this, &MainWindow::addFolder);
    connect(rescan, &QPushButton::clicked, this, [this] { m_library.rescan(); });
    connect(&m_library, &GameLibrary::scanned, this, &MainWindow::refreshList);
    connect(m_list, &QListWidget::currentRowChanged, this, &MainWindow::showGame);

    m_runTimer = new QTimer(this);
    m_runTimer->setInterval(1000);
    connect(m_runTimer, &QTimer::timeout, this, &MainWindow::watchRunning);

    if (!QFileInfo::exists(m_library.loaderPath()))
        QMessageBox::warning(this, "LindLoader", "linuxloader.exe was not found next to LindLoader.exe.");
    m_library.rescan();
}

QWidget *MainWindow::buildLibraryPanel()
{
    auto *panel = new QWidget;
    panel->setObjectName("libraryPanel");
    panel->setFixedWidth(360);
    auto *l = new QVBoxLayout(panel);
    l->setContentsMargins(0, 18, 0, 12);
    l->setSpacing(8);

    auto *head = new QHBoxLayout;
    head->setContentsMargins(22, 0, 22, 0);
    auto *label = new QLabel("LIBRARY");
    label->setObjectName("sectionLabel");
    m_count = new QLabel;
    m_count->setObjectName("muted");
    head->addWidget(label);
    head->addStretch();
    head->addWidget(m_count);
    l->addLayout(head);

    m_list = new QListWidget;
    m_list->setItemDelegate(new GameDelegate(m_list));
    m_list->setMouseTracking(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    l->addWidget(m_list, 1);
    return panel;
}

QWidget *MainWindow::buildEmptyState()
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->addStretch();
    auto *title = new QLabel("No games yet");
    title->setObjectName("emptyTitle");
    title->setAlignment(Qt::AlignCenter);
    auto *text = new QLabel("Add a folder that contains your Lindbergh game dumps.\nLindLoader finds and identifies every game inside it.");
    text->setObjectName("muted");
    text->setAlignment(Qt::AlignCenter);
    auto *btn = new QPushButton("Add Folder");
    btn->setObjectName("primary");
    btn->setFixedWidth(180);
    connect(btn, &QPushButton::clicked, this, &MainWindow::addFolder);
    l->addWidget(title);
    l->addSpacing(6);
    l->addWidget(text);
    l->addSpacing(18);
    l->addWidget(btn, 0, Qt::AlignHCenter);
    l->addStretch();
    return w;
}

QWidget *MainWindow::settingsRow(const QString &label, QWidget *field, const QString &hint)
{
    auto *row = new QWidget;
    row->setObjectName("settingsRow");
    auto *l = new QHBoxLayout(row);
    l->setContentsMargins(18, 10, 18, 10);
    auto *text = new QVBoxLayout;
    text->setSpacing(1);
    auto *name = new QLabel(label);
    name->setObjectName("rowLabel");
    text->addWidget(name);
    if (!hint.isEmpty())
    {
        auto *h = new QLabel(hint);
        h->setObjectName("rowHint");
        text->addWidget(h);
    }
    l->addLayout(text, 1);
    field->setMinimumWidth(220);
    l->addWidget(field);
    return row;
}

QWidget *MainWindow::buildDetailPanel()
{
    auto *w = new QWidget;
    w->setObjectName("detail");
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(36, 30, 36, 24);
    l->setSpacing(0);

    m_title = new QLabel;
    m_title->setObjectName("gameTitle");
    m_title->setWordWrap(true);
    l->addWidget(m_title);
    l->addSpacing(6);

    auto *metaRow = new QHBoxLayout;
    m_meta = new QLabel;
    m_meta->setObjectName("muted");
    m_badge = new QLabel;
    metaRow->addWidget(m_meta);
    metaRow->addSpacing(10);
    metaRow->addWidget(m_badge);
    metaRow->addStretch();
    l->addLayout(metaRow);
    l->addSpacing(4);
    m_path = new QLabel;
    m_path->setObjectName("path");
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(m_path);
    l->addSpacing(22);

    auto *actions = new QHBoxLayout;
    m_play = new QPushButton("▶  Play");
    m_play->setObjectName("play");
    m_play->setCursor(Qt::PointingHandCursor);
    auto *open = new QPushButton("Open Folder");
    open->setObjectName("secondary");
    m_status = new QLabel;
    m_status->setObjectName("muted");
    actions->addWidget(m_play);
    actions->addSpacing(8);
    actions->addWidget(open);
    actions->addSpacing(14);
    actions->addWidget(m_status);
    actions->addStretch();
    l->addLayout(actions);
    l->addSpacing(30);

    auto *displayLabel = new QLabel("DISPLAY");
    displayLabel->setObjectName("sectionLabel");
    l->addWidget(displayLabel);
    l->addSpacing(8);

    auto *card = new QWidget;
    card->setObjectName("card");
    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(0, 4, 0, 4);
    cl->setSpacing(0);

    m_resolution = new QComboBox;
    for (const auto &r : kResolutions)
        m_resolution->addItem(r.label, QStringList{r.width, r.height});
    m_window = new QComboBox;
    m_window->addItem("Fullscreen", "true");
    m_window->addItem("Windowed", "false");
    m_vsync = new QComboBox;
    m_vsync->addItem("Auto  ·  recommended", "AUTO");
    m_vsync->addItem("On", "1");
    m_vsync->addItem("Off", "0");
    m_aspect = new QCheckBox;
    m_fps = new QCheckBox;

    cl->addWidget(settingsRow("Resolution", m_resolution, "What the game renders at"));
    cl->addWidget(settingsRow("Display Mode", m_window));
    cl->addWidget(settingsRow("Vertical Sync", m_vsync, "Stops tearing and keeps 60 fps"));
    cl->addWidget(settingsRow("Keep Aspect Ratio", m_aspect, "Black bars instead of stretching"));
    cl->addWidget(settingsRow("Show FPS", m_fps));
    l->addWidget(card);
    l->addSpacing(26);

    auto *controlsLabel = new QLabel("CONTROLS");
    controlsLabel->setObjectName("sectionLabel");
    l->addWidget(controlsLabel);
    l->addSpacing(8);
    auto *ccard = new QWidget;
    ccard->setObjectName("card");
    auto *ccl = new QHBoxLayout(ccard);
    ccl->setContentsMargins(18, 14, 18, 14);
    auto *ctext = new QLabel("Controller mapping is coming next. Until then, controls are in controls.ini.");
    ctext->setObjectName("muted");
    ctext->setWordWrap(true);
    auto *cbtn = new QPushButton("Open controls.ini");
    cbtn->setObjectName("ghost");
    ccl->addWidget(ctext, 1);
    ccl->addWidget(cbtn);
    l->addWidget(ccard);
    l->addStretch();

    connect(m_play, &QPushButton::clicked, this, &MainWindow::play);
    connect(open, &QPushButton::clicked, this, [this] {
        if (const Game *g = currentGame())
            QDesktopServices::openUrl(QUrl::fromLocalFile(g->configDir));
    });
    connect(cbtn, &QPushButton::clicked, this, [this] {
        const Game *g = currentGame();
        QString error;
        if (g && m_library.ensureConfig(*g, &error))
            QDesktopServices::openUrl(QUrl::fromLocalFile(g->controlsPath()));
    });
    connect(m_resolution, &QComboBox::currentIndexChanged, this, [this] {
        const QStringList wh = m_resolution->currentData().toStringList();
        if (wh.size() == 2)
        {
            saveSetting("WIDTH", wh[0]);
            saveSetting("HEIGHT", wh[1]);
        }
    });
    connect(m_window, &QComboBox::currentIndexChanged, this,
            [this] { saveSetting("FULLSCREEN", m_window->currentData().toString()); });
    connect(m_vsync, &QComboBox::currentIndexChanged, this,
            [this] { saveSetting("VSYNC", m_vsync->currentData().toString()); });
    connect(m_aspect, &QCheckBox::toggled, this,
            [this](bool on) { saveSetting("KEEP_ASPECT_RATIO", on ? "true" : "false"); });
    connect(m_fps, &QCheckBox::toggled, this,
            [this](bool on) { saveSetting("FPS_OVERLAY_ENABLED", on ? "true" : "false"); });
    return w;
}

const Game *MainWindow::currentGame() const
{
    const int row = m_list->currentRow();
    if (row < 0)
        return nullptr;
    const int i = m_list->item(row)->data(RoleIndex).toInt();
    return (i >= 0 && i < m_library.games().size()) ? &m_library.games()[i] : nullptr;
}

void MainWindow::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Add a folder with Lindbergh games");
    if (dir.isEmpty())
        return;
    m_library.addFolder(dir);
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
        auto *item = new QListWidgetItem(g.title);
        QStringList sub;
        for (const QString &s : {g.gameId, g.dvp, g.year})
            if (!s.isEmpty())
                sub << s;
        item->setData(Qt::UserRole, sub.join("  ·  "));
        item->setData(RoleIndex, i);
        item->setData(Qt::UserRole + 2, g.clean);
        m_list->addItem(item);
        if (g.gameDir == previous)
            m_list->setCurrentItem(item);
    }
    m_count->setText(games.isEmpty() ? QString() : QString::number(games.size()));
    if (m_list->currentRow() < 0 && !games.isEmpty())
        m_list->setCurrentRow(0);
    m_stack->setCurrentIndex(games.isEmpty() ? 0 : 1);
}

void MainWindow::showGame(int)
{
    const Game *g = currentGame();
    if (!g)
        return;
    m_title->setText(g->title);
    QStringList meta;
    for (const QString &s : {g->gameId, g->dvp, g->year})
        if (!s.isEmpty())
            meta << s;
    if (!g->resolutions.isEmpty())
        meta << "Native " + QString(g->resolutions).replace("x", " × ");
    m_meta->setText(meta.join("   ·   "));
    m_badge->setText(g->clean ? "CLEAN DUMP" : "MODIFIED ELF");
    m_badge->setObjectName(g->clean ? "badgeClean" : "badgeModified");
    m_badge->style()->unpolish(m_badge);
    m_badge->style()->polish(m_badge);
    m_path->setText(QDir::toNativeSeparators(g->gameDir));
    m_status->clear();
    loadSettings();
}

void MainWindow::loadSettings()
{
    const Game *g = currentGame();
    QString error;
    if (!g || !m_library.ensureConfig(*g, &error) || !m_ini.load(g->configPath()))
    {
        m_status->setText(error);
        return;
    }
    m_loading = true;
    const QString w = m_ini.value("WIDTH", "AUTO"), h = m_ini.value("HEIGHT", "AUTO");
    int res = m_resolution->findData(QStringList{w, h});
    if (res < 0)
    {
        m_resolution->addItem(QStringLiteral("%1 × %2  ·  custom").arg(w, h), QStringList{w, h});
        res = m_resolution->count() - 1;
    }
    m_resolution->setCurrentIndex(res);
    m_window->setCurrentIndex(m_ini.value("FULLSCREEN", "true") == "true" ? 0 : 1);
    const int vs = m_vsync->findData(m_ini.value("VSYNC", "AUTO").toUpper());
    m_vsync->setCurrentIndex(vs < 0 ? 0 : vs);
    m_aspect->setChecked(m_ini.value("KEEP_ASPECT_RATIO", "true") == "true");
    m_fps->setChecked(m_ini.value("FPS_OVERLAY_ENABLED", "false") == "true");
    m_loading = false;
}

void MainWindow::saveSetting(const QString &key, const QString &value)
{
    if (m_loading || !currentGame())
        return;
    m_ini.setValue(key, value);
    m_status->setText(m_ini.save() ? "Saved" : "Could not save settings");
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
    m_play->setEnabled(false);
    m_play->setText("Running…");
    m_status->setText(QString());
    m_runTimer->start();
}

void MainWindow::watchRunning()
{
    if (m_process && WaitForSingleObject(m_process, 0) == WAIT_OBJECT_0)
    {
        CloseHandle(m_process);
        m_process = nullptr;
        m_runTimer->stop();
        m_play->setEnabled(true);
        m_play->setText("▶  Play");
    }
}
