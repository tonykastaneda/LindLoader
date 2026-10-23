#include "GamePropertiesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "ControllerSettingsWidget.h"
#include "Theme.h"

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

QWidget *page(QVBoxLayout **layout)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    *layout = l;
    return w;
}
} // namespace

GamePropertiesDialog::GamePropertiesDialog(const Game &game, GameLibrary &library, Page page, QWidget *parent)
    : QDialog(parent), m_game(game), m_library(library)
{
    setWindowTitle(game.title + " Properties");
    resize(1100, 720);

    QString error;
    if (m_library.ensureConfig(m_game, &error))
        m_ini.load(m_game.configPath());

    auto *categories = new QListWidget;
    categories->setFixedWidth(170);
    categories->setIconSize(QSize(20, 20));
    categories->addItem(new QListWidgetItem(Theme::icon("info"), "Summary"));
    categories->addItem(new QListWidgetItem(Theme::icon("monitor"), "Graphics"));
    categories->addItem(new QListWidgetItem(Theme::icon("settings"), "Emulation"));
    categories->addItem(new QListWidgetItem(Theme::icon("gamepad"), "Controls"));

    auto *pages = new QStackedWidget;
    pages->addWidget(buildSummaryPage());
    pages->addWidget(buildGraphicsPage());
    pages->addWidget(buildEmulationPage());
    pages->addWidget(new ControllerSettingsWidget(m_game, m_library));
    // Follow the highlighted category; selecting one through accessibility tools doesn't move the current row.
    connect(categories, &QListWidget::itemSelectionChanged, pages, [categories, pages] {
        if (!categories->selectedItems().isEmpty())
            pages->setCurrentIndex(categories->row(categories->selectedItems().first()));
    });
    categories->setCurrentRow(page);

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
    QVBoxLayout *l;
    QWidget *w = page(&l);
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
    auto *open = new QPushButton(Theme::icon("folder-open"), "Open Game Folder");
    connect(open, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_game.gameDir)); });
    ff->addRow("", open);
    l->addWidget(files);
    l->addStretch();
    return w;
}

QWidget *GamePropertiesDialog::buildGraphicsPage()
{
    auto *tabs = new QTabWidget;

    // Display
    QVBoxLayout *dl;
    QWidget *display = page(&dl);
    auto *box = new QGroupBox("Display");
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
    connect(resolution, &QComboBox::currentIndexChanged, this, [this, resolution] {
        const QStringList wh = resolution->currentData().toStringList();
        save("WIDTH", wh.value(0));
        save("HEIGHT", wh.value(1));
    });
    form->addRow("Resolution:", resolution);
    form->addRow("Vertical Sync:", combo("VSYNC", "AUTO", {{"Auto (Recommended)", "AUTO"}, {"On", "1"}, {"Off", "0"}}));
    form->addRow(check("Start Fullscreen", "FULLSCREEN", false));
    form->addRow(check("Keep Aspect Ratio (black bars instead of stretching)", "KEEP_ASPECT_RATIO", true));
    // The loader widens SEGA Race TV's and OutRun 2 SP SDX's 3D view to a wide resolution (the HUD keeps its
    // shape). New in the loader's [Display], so it may not be in the file yet: written there by name.
    if (m_game.gameId == "SBPF" || m_game.gameId == "SBMB")
    {
        auto *wide = new QCheckBox("Widescreen (wider 3D view at a wide resolution such as 1920 x 1080)");
        wide->setChecked(m_ini.value("Display", "WIDESCREEN", "true").toLower() == "true");
        connect(wide, &QCheckBox::toggled, this, [this](bool on) {
            if (m_loading)
                return;
            m_ini.setValue("Display", "WIDESCREEN", on ? "true" : "false");
            m_ini.save();
        });
        form->addRow(wide);
    }
    // The loader removes Virtua Fighter 5 FS Rev B's black band behind the credit text (it hides the top of
    // the 3D at other resolutions). New in the loader's [Display], so written there by name like Widescreen.
    if (m_game.shortTitle == "vf5-fs-rev-b")
    {
        auto *band = new QCheckBox("Hide Credit Band (3D view uses the full height, credit text drawn over it)");
        band->setChecked(m_ini.value("Display", "HIDE_CREDIT_BAND", "true").toLower() == "true");
        connect(band, &QCheckBox::toggled, this, [this](bool on) {
            if (m_loading)
                return;
            m_ini.setValue("Display", "HIDE_CREDIT_BAND", on ? "true" : "false");
            m_ini.save();
        });
        form->addRow(band);
    }
    dl->addWidget(box);

    auto *frame = new QGroupBox("Frame Rate");
    auto *ff = new QFormLayout(frame);
    auto *target = new QDoubleSpinBox;
    target->setRange(30.0, 240.0);
    target->setDecimals(2);
    target->setSuffix(" FPS");
    target->setValue(m_ini.value("FPS_TARGET", "60.0").toDouble());
    connect(target, &QDoubleSpinBox::valueChanged, this, [this](double v) { save("FPS_TARGET", QString::number(v, 'f', 1)); });
    QCheckBox *limiter = check("Limit Frame Rate", "FPS_LIMITER_ENABLED", true);
    target->setEnabled(limiter->isChecked());
    connect(limiter, &QCheckBox::toggled, target, &QWidget::setEnabled);
    ff->addRow(limiter);
    ff->addRow("Target:", target);
    dl->addWidget(frame);
    dl->addStretch();
    tabs->addTab(display, "Display");

    // On-Screen Display
    QVBoxLayout *ol;
    QWidget *osd = page(&ol);
    auto *overlay = new QGroupBox("On-Screen Display");
    auto *of = new QFormLayout(overlay);
    of->addRow(check("Show FPS", "FPS_OVERLAY_ENABLED", false));
    of->addRow("Position:", combo("FPS_OVERLAY_POSITION", "0",
                                  {{"Top Left", "0"}, {"Top Right", "1"}, {"Bottom Left", "2"}, {"Bottom Right", "3"}}));
    ol->addWidget(overlay);
    auto *cursor = new QGroupBox("Mouse Cursor");
    auto *cl = new QVBoxLayout(cursor);
    cl->addWidget(check("Hide Mouse Cursor", "HIDE_CURSOR", true));
    ol->addWidget(cursor);
    ol->addStretch();
    tabs->addTab(osd, "On-Screen Display");

    // Advanced
    QVBoxLayout *al;
    QWidget *advanced = page(&al);
    auto *rendering = new QGroupBox("Rendering");
    auto *rl = new QVBoxLayout(rendering);
    rl->addWidget(check("Boost Render Resolution (House of the Dead 4, 2 Spicy, Harley, Rambo, HOD EX, ID4/ID5, LGJ)",
                        "BOOST_RENDER_RES", true));
    rl->addWidget(check("Use the Game's Original Fonts", "DISABLE_BUILTIN_FONT", false));
    rl->addWidget(check("Use the Game's Original Logos", "DISABLE_BUILTIN_LOGOS", false));
    al->addWidget(rendering);
    al->addStretch();
    tabs->addTab(advanced, "Advanced");

    return tabs;
}

QWidget *GamePropertiesDialog::buildEmulationPage()
{
    QVBoxLayout *l;
    QWidget *w = page(&l);
    auto *box = new QGroupBox("System");
    auto *form = new QFormLayout(box);
    form->addRow("Region:", combo("REGION", "EX", {{"Japan", "JP"}, {"USA", "US"}, {"Export", "EX"}}));
    form->addRow("Free Play:", combo("FREEPLAY", "none", {{"Game Default", "none"}, {"On", "true"}, {"Off", "false"}}));
    form->addRow("Lindbergh Colour:", combo("LINDBERGH_COLOUR", "YELLOW",
                                            {{"Yellow", "YELLOW"},
                                             {"Red", "RED"},
                                             {"Blue", "BLUE"},
                                             {"Silver", "SILVER"},
                                             {"Red EX", "REDEX"}}));
    l->addWidget(box);
    l->addStretch();
    return w;
}

QCheckBox *GamePropertiesDialog::check(const QString &text, const QString &key, bool fallback)
{
    auto *box = new QCheckBox(text);
    box->setChecked(m_ini.value(key, fallback ? "true" : "false").toLower() == "true");
    connect(box, &QCheckBox::toggled, this, [this, key](bool on) { save(key, on ? "true" : "false"); });
    return box;
}

QComboBox *GamePropertiesDialog::combo(const QString &key, const QString &fallback, const Choices &choices)
{
    auto *box = new QComboBox;
    for (const auto &c : choices)
        box->addItem(c.first, c.second);
    const QString current = m_ini.value(key, fallback).remove('"');
    int i = box->findData(current, Qt::UserRole, Qt::MatchFixedString);
    if (i < 0)
    {
        box->addItem(current + " (custom)", current);
        i = box->count() - 1;
    }
    box->setCurrentIndex(i);
    connect(box, &QComboBox::currentIndexChanged, this, [this, key, box] { save(key, box->currentData().toString()); });
    return box;
}

void GamePropertiesDialog::save(const QString &key, const QString &value)
{
    if (m_loading)
        return;
    m_ini.setValue(key, value);
    m_ini.save();
}
