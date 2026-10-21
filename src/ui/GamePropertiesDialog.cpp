#include "GamePropertiesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
struct Resolution
{
    const char *label;
    const char *width;
    const char *height;
};

const Resolution kResolutions[] = {
    {"Native (game default)", "AUTO", "AUTO"},
    {"1280 x 720 (16:9)", "1280", "720"},
    {"1920 x 1080 (16:9)", "1920", "1080"},
    {"2560 x 1440 (16:9)", "2560", "1440"},
    {"3840 x 2160 (16:9)", "3840", "2160"},
    {"640 x 480 (4:3)", "640", "480"},
    {"1440 x 1080 (4:3)", "1440", "1080"},
    {"2880 x 2160 (4:3)", "2880", "2160"},
};

QLineEdit *readOnly(const QString &text)
{
    auto *e = new QLineEdit(text);
    e->setReadOnly(true);
    return e;
}
} // namespace

GamePropertiesDialog::GamePropertiesDialog(const Game &game, GameLibrary &library, QWidget *parent)
    : QDialog(parent), m_game(game), m_library(library)
{
    setWindowTitle(game.title + " Properties");
    resize(760, 520);

    QString error;
    if (m_library.ensureConfig(m_game, &error))
        m_ini.load(m_game.configPath());

    auto *categories = new QListWidget;
    categories->setFixedWidth(170);
    categories->setIconSize(QSize(20, 20));
    categories->addItem(new QListWidgetItem(QIcon(":/icons/info.svg"), "Summary"));
    categories->addItem(new QListWidgetItem(QIcon(":/icons/monitor.svg"), "Display"));
    categories->addItem(new QListWidgetItem(QIcon(":/icons/gamepad.svg"), "Controls"));

    auto *pages = new QStackedWidget;
    pages->addWidget(buildSummaryPage());
    pages->addWidget(buildDisplayPage());
    pages->addWidget(buildControlsPage());
    connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    categories->setCurrentRow(0);

    auto *body = new QHBoxLayout;
    body->addWidget(categories);
    body->addWidget(pages, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *root = new QVBoxLayout(this);
    root->addLayout(body, 1);
    root->addWidget(buttons);
    m_loading = false;
}

QWidget *GamePropertiesDialog::buildSummaryPage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    auto *box = new QGroupBox("Game");
    auto *form = new QFormLayout(box);
    form->addRow("Title:", readOnly(m_game.title));
    form->addRow("Game ID:", readOnly(m_game.gameId));
    form->addRow("DVP:", readOnly(m_game.dvp));
    form->addRow("Year:", readOnly(m_game.year));
    form->addRow("Native resolution:", readOnly(m_game.resolutions));
    form->addRow("Executable:", readOnly(m_game.elf));
    form->addRow("CRC32:", readOnly(m_game.crc));
    form->addRow("Dump:", readOnly(m_game.clean ? "Clean (verified)" : "Modified (not a known clean dump)"));
    l->addWidget(box);

    auto *files = new QGroupBox("Files");
    auto *ff = new QFormLayout(files);
    ff->addRow("Game folder:", readOnly(QDir::toNativeSeparators(m_game.gameDir)));
    ff->addRow("Settings:", readOnly(QDir::toNativeSeparators(m_game.configPath())));
    ff->addRow("Controls:", readOnly(QDir::toNativeSeparators(m_game.controlsPath())));
    auto *open = new QPushButton(QIcon(":/icons/folder-open.svg"), "Open Game Folder");
    connect(open, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_game.gameDir)); });
    ff->addRow("", open);
    l->addWidget(files);
    l->addStretch();
    return page;
}

QWidget *GamePropertiesDialog::buildDisplayPage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);

    auto *box = new QGroupBox("Rendering");
    auto *form = new QFormLayout(box);
    auto *resolution = new QComboBox;
    for (const auto &r : kResolutions)
        resolution->addItem(r.label, QStringList{r.width, r.height});
    const QString w = m_ini.value("WIDTH", "AUTO"), h = m_ini.value("HEIGHT", "AUTO");
    int res = resolution->findData(QStringList{w, h});
    if (res < 0)
    {
        resolution->addItem(QStringLiteral("%1 x %2 (custom)").arg(w, h), QStringList{w, h});
        res = resolution->count() - 1;
    }
    resolution->setCurrentIndex(res);
    form->addRow("Resolution:", resolution);

    auto *vsync = new QComboBox;
    vsync->addItem("Auto (Recommended)", "AUTO");
    vsync->addItem("On", "1");
    vsync->addItem("Off", "0");
    const int vs = vsync->findData(m_ini.value("VSYNC", "AUTO").toUpper());
    vsync->setCurrentIndex(vs < 0 ? 0 : vs);
    form->addRow("Vertical Sync:", vsync);
    l->addWidget(box);

    auto *screen = new QGroupBox("Screen");
    auto *sl = new QVBoxLayout(screen);
    auto *fullscreen = new QCheckBox("Start Fullscreen");
    fullscreen->setChecked(m_ini.value("FULLSCREEN", "true") == "true");
    auto *aspect = new QCheckBox("Keep Aspect Ratio (black bars instead of stretching)");
    aspect->setChecked(m_ini.value("KEEP_ASPECT_RATIO", "true") == "true");
    auto *fps = new QCheckBox("Show FPS Overlay");
    fps->setChecked(m_ini.value("FPS_OVERLAY_ENABLED", "false") == "true");
    sl->addWidget(fullscreen);
    sl->addWidget(aspect);
    sl->addWidget(fps);
    l->addWidget(screen);
    l->addStretch();

    connect(resolution, &QComboBox::currentIndexChanged, this, [this, resolution] {
        const QStringList wh = resolution->currentData().toStringList();
        save("WIDTH", wh.value(0));
        save("HEIGHT", wh.value(1));
    });
    connect(vsync, &QComboBox::currentIndexChanged, this, [this, vsync] { save("VSYNC", vsync->currentData().toString()); });
    connect(fullscreen, &QCheckBox::toggled, this, [this](bool on) { save("FULLSCREEN", on ? "true" : "false"); });
    connect(aspect, &QCheckBox::toggled, this, [this](bool on) { save("KEEP_ASPECT_RATIO", on ? "true" : "false"); });
    connect(fps, &QCheckBox::toggled, this, [this](bool on) { save("FPS_OVERLAY_ENABLED", on ? "true" : "false"); });
    return page;
}

QWidget *GamePropertiesDialog::buildControlsPage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    auto *box = new QGroupBox("Controller Mapping");
    auto *bl = new QVBoxLayout(box);
    auto *text = new QLabel("Button mapping is coming in a later version. Until then, this game's controls are in its "
                            "controls.ini.");
    text->setWordWrap(true);
    auto *open = new QPushButton("Open controls.ini");
    connect(open, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_game.controlsPath())); });
    bl->addWidget(text);
    bl->addWidget(open, 0, Qt::AlignLeft);
    l->addWidget(box);
    l->addStretch();
    return page;
}

void GamePropertiesDialog::save(const QString &key, const QString &value)
{
    if (m_loading)
        return;
    m_ini.setValue(key, value);
    m_ini.save();
}
