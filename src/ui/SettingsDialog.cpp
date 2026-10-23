#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <SDL3/SDL.h>

#include "AppSettings.h"
#include "GameLibrary.h"
#include "Theme.h"
#include "UnpackImages.h"

namespace
{
QWidget *page(QVBoxLayout **layout)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(4, 0, 8, 0);
    *layout = l;
    return w;
}

QPushButton *iconButton(const QString &icon, const QString &text)
{
    return new QPushButton(Theme::icon(icon), text.isEmpty() ? QString() : " " + text);
}
} // namespace

SettingsDialog::SettingsDialog(GameLibrary &library, Page page, QWidget *parent)
    : SettingsWindow(library, "Lindy Settings", parent)
{
    addPage("sliders", "Interface", "These options control how Lindy looks and behaves.\n\n"
                                    "Mouse over an option for more information.",
            buildInterfacePage());
    addPage("folder-open", "Game List",
            "The folders above are searched for Lindbergh games to fill the game list, with their subfolders. "
            "Excluded paths are skipped.",
            buildGameListPage());
    addPage("speaker", "Audio", "These options control the sound of every game.\n\nMouse over an option for more information.",
            buildAudioPage());
    addPage("globe", "Network",
            "These options are the network addresses of linked cabinets: games that play together over a network, "
            "such as OutRun 2, Initial D and R-Tuned. They're the same for every game.",
            buildNetworkPage());
    addPage("folder-add", "Folders", "These options control where Lindy keeps covers and where games keep their save data and cards.",
            buildFoldersPage());
    setDefaultsGroups({"interface", "library", "audio", "network", "folders"});
    showPage(page);
}

QWidget *SettingsDialog::buildInterfacePage()
{
    QVBoxLayout *l;
    QWidget *w = page(&l);

    auto *behaviour = new QGroupBox("Behaviour");
    auto *bg = new QGridLayout(behaviour);
    bg->addWidget(check("Confirm Exit While a Game Is Running", "interface/confirmExit",
                        "Asks before closing Lindy while a game is running. Closing Lindy also closes the game."),
                  0, 0);
    bg->addWidget(check("Inhibit Screensaver", "interface/inhibitScreensaver",
                        "Keeps the screensaver and display sleep away while a game is running."),
                  0, 1);
    l->addWidget(behaviour);

    auto *display = new QGroupBox("Game Display");
    auto *dg = new QGridLayout(display);
    dg->addWidget(check("Hide Main Window When Running", "interface/hideMainWindow",
                        "Hides Lindy while a game is running and brings it back when the game closes."),
                  0, 0);
    l->addWidget(display);

    auto *appearance = new QGroupBox("Appearance");
    auto *af = new QFormLayout(appearance);
    auto *theme = new QComboBox;
    for (const auto &[label, name] : Theme::choices())
        theme->addItem(label, name);
    onLoad([this, theme] { theme->setCurrentIndex(qMax(0, theme->findData(m_library.setting("interface/theme")))); });
    connect(theme, &QComboBox::currentIndexChanged, this, [this, theme] {
        changed("interface/theme", theme->currentData());
        if (!m_loading)
            Theme::apply(theme->currentData().toString());
    });
    help(theme, "Theme", "Lindy's colours: Dark, Light, or the Windows style.");
    af->addRow("Theme:", theme);

    // Game list background: an image behind the list and the grid.
    auto *bgRow = new QHBoxLayout;
    auto *browse = new QPushButton("Browse...");
    auto *clear = iconButton("trash", QString());
    clear->setToolTip("Remove the background");
    auto *opacity = new QSpinBox;
    opacity->setRange(0, 100);
    opacity->setSuffix("%");
    auto *scaling = new QComboBox;
    for (const auto &[label, value] : QList<QPair<QString, QString>>{
             {"Fill", "fill"}, {"Fit", "fit"}, {"Stretch", "stretch"}, {"Center", "center"}, {"Tile", "tile"}})
        scaling->addItem(label, value);
    auto *bgName = new QLabel;
    onLoad([this, opacity, scaling, bgName] {
        opacity->setValue(m_library.setting("interface/backgroundOpacity").toInt());
        scaling->setCurrentIndex(qMax(0, scaling->findData(m_library.setting("interface/backgroundScaling"))));
        const QString file = m_library.setting("interface/background").toString();
        bgName->setText(file.isEmpty() ? "None" : QFileInfo(file).fileName());
        bgName->setToolTip(QDir::toNativeSeparators(file));
    });
    connect(browse, &QPushButton::clicked, this, [this, bgName] {
        const QString file = QFileDialog::getOpenFileName(this, "Game List Background", QString(),
                                                          "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (file.isEmpty())
            return;
        changed("interface/background", file);
        reload();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        changed("interface/background", QString());
        reload();
    });
    connect(opacity, &QSpinBox::valueChanged, this, [this](int v) { changed("interface/backgroundOpacity", v); });
    connect(scaling, &QComboBox::currentIndexChanged, this,
            [this, scaling] { changed("interface/backgroundScaling", scaling->currentData()); });
    bgRow->addWidget(bgName, 1);
    bgRow->addWidget(browse);
    bgRow->addWidget(clear);
    bgRow->addWidget(new QLabel("Opacity:"));
    bgRow->addWidget(opacity);
    bgRow->addWidget(new QLabel("Scaling:"));
    bgRow->addWidget(scaling);
    const QString bgHelp = "An image behind the game list and the game grid. Opacity sets how strongly it shows.";
    for (QWidget *x : {static_cast<QWidget *>(browse), static_cast<QWidget *>(opacity), static_cast<QWidget *>(scaling)})
        help(x, "Game List Background", bgHelp);
    af->addRow("Game List Background:", bgRow);
    l->addWidget(appearance);
    l->addStretch();
    return w;
}

QWidget *SettingsDialog::buildGameListPage()
{
    QVBoxLayout *l;
    QWidget *w = page(&l);
    auto *box = new QGroupBox("Game Scanning");
    auto *bl = new QVBoxLayout(box);

    // Search directories
    auto *addDir = iconButton("folder-add", "Add...");
    auto *removeDir = iconButton("trash", "Remove");
    auto *dirHeader = new QHBoxLayout;
    dirHeader->addWidget(new QLabel("Search Directories (will be scanned for games)"), 1);
    dirHeader->addWidget(addDir);
    dirHeader->addWidget(removeDir);
    m_folders = new QTreeWidget;
    m_folders->setHeaderLabels({"Search Directory"});
    m_folders->setRootIsDecorated(false);
    onLoad([this] {
        m_folders->clear();
        for (const QString &f : m_library.folders())
            new QTreeWidgetItem(m_folders, {QDir::toNativeSeparators(f)});
    });
    connect(addDir, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, "Add Search Directory");
        if (dir.isEmpty())
            return;
        m_library.addFolder(dir);
        reload();
        rescan();
    });
    connect(removeDir, &QPushButton::clicked, this, [this] {
        if (!m_folders->currentItem())
            return;
        m_library.removeFolder(QDir::fromNativeSeparators(m_folders->currentItem()->text(0)));
        reload();
        rescan();
    });
    help(m_folders, "Search Directories", "Folders with Lindbergh games. Each is searched with its subfolders.");
    bl->addLayout(dirHeader);
    bl->addWidget(m_folders, 1);

    // Excluded paths
    auto *excludeDir = iconButton("folder-add", "Directory...");
    auto *excludeFile = iconButton("folder-add", "File...");
    auto *removeExcluded = iconButton("trash", "Remove");
    auto *exHeader = new QHBoxLayout;
    exHeader->addWidget(new QLabel("Excluded Paths (will not be scanned)"), 1);
    exHeader->addWidget(excludeDir);
    exHeader->addWidget(excludeFile);
    exHeader->addWidget(removeExcluded);
    m_excluded = new QTreeWidget;
    m_excluded->setHeaderHidden(true);
    m_excluded->setRootIsDecorated(false);
    onLoad([this] {
        m_excluded->clear();
        for (const QString &p : m_library.setting("library/excluded").toStringList())
            new QTreeWidgetItem(m_excluded, {QDir::toNativeSeparators(p)});
    });
    auto exclude = [this](const QString &path) {
        if (path.isEmpty())
            return;
        QStringList list = m_library.setting("library/excluded").toStringList();
        if (!list.contains(path, Qt::CaseInsensitive))
            list << QDir::cleanPath(path);
        changed("library/excluded", list);
        reload();
        rescan();
    };
    connect(excludeDir, &QPushButton::clicked, this,
            [this, exclude] { exclude(QFileDialog::getExistingDirectory(this, "Exclude Directory")); });
    connect(excludeFile, &QPushButton::clicked, this,
            [this, exclude] { exclude(QFileDialog::getOpenFileName(this, "Exclude Game Executable")); });
    connect(removeExcluded, &QPushButton::clicked, this, [this] {
        if (!m_excluded->currentItem())
            return;
        QStringList list = m_library.setting("library/excluded").toStringList();
        list.removeAll(QDir::fromNativeSeparators(m_excluded->currentItem()->text(0)));
        changed("library/excluded", list);
        reload();
        rescan();
    });
    help(m_excluded, "Excluded Paths",
         "A folder here hides the games inside it; a game's executable hides that game.");
    bl->addLayout(exHeader);
    bl->addWidget(m_excluded, 1);

    auto *rescanButton = iconButton("refresh", "Rescan All Games");
    connect(rescanButton, &QPushButton::clicked, this, &SettingsDialog::rescan);
    help(rescanButton, "Rescan All Games", "Searches the directories again and identifies every game.");
    bl->addWidget(rescanButton, 0, Qt::AlignRight);
    l->addWidget(box, 1);

    // Disk images are unpacked on request, never run.
    auto *images = new QGroupBox("Disk Images (.bin)");
    auto *il = new QHBoxLayout(images);
    auto *after = new QComboBox;
    after->addItem("Ask", "ask");
    after->addItem("Keep the Images", "keep");
    after->addItem("Delete the Images", "delete");
    onLoad([this, after] { after->setCurrentIndex(qMax(0, after->findData(m_library.setting("library/afterUnpack")))); });
    connect(after, &QComboBox::currentIndexChanged, this,
            [this, after] { changed("library/afterUnpack", after->currentData()); });
    help(after, "After Unpacking",
         "What happens to disk images once they're unpacked. Lindy runs the unpacked folder, so the image is "
         "only needed to unpack it again.");
    auto *unpack = iconButton("folder-open", "Unpack (.bin)...");
    connect(unpack, &QPushButton::clicked, this, [this] { unpackDiskImages(m_library, this); });
    help(unpack, "Unpack (.bin)",
         "Unpacks disk images (Batocera's .bin files) into a folder next to each. Lindy doesn't run images "
         "directly.");
    il->addWidget(new QLabel("After Unpacking:"));
    il->addWidget(after);
    il->addStretch();
    il->addWidget(unpack);
    l->addWidget(images);
    return w;
}

void SettingsDialog::rescan()
{
    m_library.rescan();
}

QWidget *SettingsDialog::buildAudioPage()
{
    QVBoxLayout *l;
    QWidget *w = page(&l);

    auto *config = new QGroupBox("Configuration");
    auto *cf = new QFormLayout(config);
    m_audioDriver = new QComboBox;
    m_audioDriver->addItem("Default", QString());
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
    {
        const QString name = QString::fromUtf8(SDL_GetAudioDriver(i));
        if (name == "wasapi")
            m_audioDriver->addItem("WASAPI", name);
        else if (name == "directsound")
            m_audioDriver->addItem("DirectSound", name);
    }
    m_audioDevice = new QComboBox;
    onLoad([this] {
        m_audioDriver->setCurrentIndex(qMax(0, m_audioDriver->findData(m_library.setting("audio/driver"))));
        fillAudioDevices();
    });
    connect(m_audioDriver, &QComboBox::currentIndexChanged, this, [this] {
        changed("audio/driver", m_audioDriver->currentData());
        if (!m_loading)
        {
            changed("audio/device", QString()); // device names differ between backends
            fillAudioDevices();
        }
    });
    connect(m_audioDevice, &QComboBox::activated, this, [this] { changed("audio/device", m_audioDevice->currentData()); });
    help(m_audioDriver, "Backend", "The Windows audio system games play through. Default is WASAPI.");
    help(m_audioDevice, "Output Device", "The speakers or headphones games play through. Default follows Windows.");
    cf->addRow("Backend:", m_audioDriver);
    cf->addRow("Output Device:", m_audioDevice);

    auto *buffer = new QSlider(Qt::Horizontal);
    buffer->setRange(0, 20); // 0 = default, then 10 ms steps
    buffer->setTickPosition(QSlider::TicksBelow);
    auto *bufferLabel = new QLabel;
    bufferLabel->setMinimumWidth(70);
    auto showBuffer = [bufferLabel](int v) { bufferLabel->setText(v == 0 ? "Default" : QStringLiteral("%1 ms").arg(v * 10)); };
    onLoad([this, buffer, showBuffer] {
        buffer->setValue(m_library.setting("audio/bufferMs").toInt() / 10);
        showBuffer(buffer->value());
    });
    connect(buffer, &QSlider::valueChanged, this, [this, showBuffer](int v) {
        showBuffer(v);
        changed("audio/bufferMs", v * 10);
    });
    help(buffer, "Buffer Size",
         "How much sound is queued ahead. Larger buffers stop crackling on a busy PC but add delay. "
         "Default lets Windows choose.");
    auto *bufferRow = new QHBoxLayout;
    bufferRow->addWidget(buffer, 1);
    bufferRow->addWidget(bufferLabel);
    cf->addRow("Buffer Size:", bufferRow);
    l->addWidget(config);

    auto *controls = new QGroupBox("Controls");
    auto *cl = new QFormLayout(controls);
    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    volume->setTickPosition(QSlider::TicksBelow);
    volume->setTickInterval(10);
    auto *volumeLabel = new QLabel;
    volumeLabel->setMinimumWidth(50);
    auto *volumeReset = new QToolButton;
    volumeReset->setIcon(Theme::icon("reset"));
    volumeReset->setToolTip("Back to 100%");
    onLoad([this, volume, volumeLabel] {
        volume->setValue(m_library.setting("audio/volume").toInt());
        volumeLabel->setText(QStringLiteral("%1%").arg(volume->value()));
    });
    connect(volume, &QSlider::valueChanged, this, [this, volumeLabel](int v) {
        volumeLabel->setText(QStringLiteral("%1%").arg(v));
        changed("audio/volume", v);
    });
    connect(volumeReset, &QToolButton::clicked, volume, [volume] { volume->setValue(100); });
    help(volume, "Volume", "The volume of every game.");
    auto *volumeRow = new QHBoxLayout;
    volumeRow->addWidget(volume, 1);
    volumeRow->addWidget(volumeLabel);
    volumeRow->addWidget(volumeReset);
    cl->addRow("Volume:", volumeRow);
    cl->addRow(check("Mute All Sound", "audio/mute", "Silences every game."));
    l->addWidget(controls);
    l->addStretch();
    return w;
}

// The playback devices of the chosen backend, as SDL (and so the loader) names them.
void SettingsDialog::fillAudioDevices()
{
    const QString driver = m_audioDriver->currentData().toString();
    const QString saved = m_library.setting("audio/device").toString();
    m_audioDevice->clear();
    m_audioDevice->addItem("Default", QString());
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, driver.isEmpty() ? nullptr : driver.toUtf8().constData());
    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        int count = 0;
        if (SDL_AudioDeviceID *ids = SDL_GetAudioPlaybackDevices(&count))
        {
            for (int i = 0; i < count; ++i)
                if (const char *name = SDL_GetAudioDeviceName(ids[i]))
                    m_audioDevice->addItem(QString::fromUtf8(name), QString::fromUtf8(name));
            SDL_free(ids);
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
    if (!saved.isEmpty() && m_audioDevice->findData(saved) < 0)
        m_audioDevice->addItem(saved + " (not connected)", saved);
    m_audioDevice->setCurrentIndex(qMax(0, m_audioDevice->findData(saved)));
}

QWidget *SettingsDialog::buildNetworkPage()
{
    QVBoxLayout *l;
    QWidget *w = page(&l);
    auto *box = new QGroupBox("Linked Cabinets");
    auto *bl = new QVBoxLayout(box);
    QCheckBox *enabled = check("Enabled", "network/enabled",
                               "Applies the network patches of linked games, with the addresses below. "
                               "Turn it off to leave the games' own network setup alone.");
    bl->addWidget(enabled);

    // This PC's addresses: what a linked game usually needs as its own.
    QStringList own;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && !a.isLinkLocal())
            own << a.toString();
    auto *mine = new QLabel("This PC's address: <b>" + (own.isEmpty() ? QString("none") : own.join(", ")) + "</b>");
    mine->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bl->addWidget(mine);

    auto *tabs = new QTabWidget;
    static const QRegularExpression ipv4("^((25[0-5]|2[0-4]\\d|1?\\d?\\d)\\.){3}(25[0-5]|2[0-4]\\d|1?\\d?\\d)$");
    auto address = [this](QFormLayout *form, const QString &label, const QString &key, const QString &helpText) {
        auto *edit = new QLineEdit;
        edit->setValidator(new QRegularExpressionValidator(ipv4, edit));
        onLoad([this, edit, key] { edit->setText(m_library.setting(key).toString()); });
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, key] {
            if (edit->hasAcceptableInput())
                changed(key, edit->text());
        });
        help(edit, label.chopped(1), helpText);
        form->addRow(label, edit);
    };
    const struct
    {
        const char *title;
        QList<QStringList> rows; // label, key, help
    } groups[] = {
        {"OutRun 2",
         {{"Address:", "network/or2Ip", "This PC's address on the link network (OutRun 2 SP SDX)."},
          {"Netmask:", "network/or2Netmask", "The link network's netmask, usually 255.255.255.0."}}},
        {"Initial D",
         {{"Seat 1:", "network/idSeat1", "The address of seat 1 (Initial D 4 and 5)."},
          {"Seat 2:", "network/idSeat2", "The address of seat 2 (Initial D 4 and 5)."}}},
        {"Harley / Hummer / R-Tuned",
         {{"Cabinet 1:", "network/cab1", "The address of cabinet 1."},
          {"Cabinet 2:", "network/cab2", "The address of cabinet 2."},
          {"Cabinet 3:", "network/cab3", "The address of cabinet 3."},
          {"Cabinet 4:", "network/cab4", "The address of cabinet 4."}}},
        {"2 Spicy",
         {{"Cabinet 1:", "network/spicy1", "The address of the first 2 Spicy cabinet."},
          {"Cabinet 2:", "network/spicy2", "The address of the second 2 Spicy cabinet."}}},
        {"SEGA Race TV", {{"Address:", "network/srtvIp", "This PC's address for SEGA Race TV's network."}}},
    };
    for (const auto &g : groups)
    {
        auto *tab = new QWidget;
        auto *form = new QFormLayout(tab);
        for (const QStringList &r : g.rows)
            address(form, r[0], r[1], r[2]);
        tabs->addTab(tab, g.title);
    }
    auto enableTabs = [tabs](bool on) { tabs->setEnabled(on); };
    onLoad([this, enableTabs] { enableTabs(m_library.setting("network/enabled").toBool()); });
    connect(enabled, &QCheckBox::toggled, tabs, enableTabs);
    bl->addWidget(tabs);
    l->addWidget(box);
    l->addStretch();
    return w;
}

QWidget *SettingsDialog::folderRow(const QString &title, const QString &description, const QString &key,
                                   const QString &defaultText)
{
    auto *box = new QGroupBox(title);
    auto *bl = new QVBoxLayout(box);
    bl->addWidget(new QLabel(description));
    auto *row = new QHBoxLayout;
    auto *path = new QLineEdit;
    path->setReadOnly(true);
    path->setPlaceholderText(defaultText);
    auto *browse = new QPushButton("Browse...");
    auto *open = new QPushButton("Open...");
    auto *reset = new QPushButton("Reset");
    onLoad([this, path, key] { path->setText(QDir::toNativeSeparators(m_library.setting(key).toString())); });
    connect(browse, &QPushButton::clicked, this, [this, key, title] {
        const QString dir = QFileDialog::getExistingDirectory(this, title);
        if (dir.isEmpty())
            return;
        changed(key, QDir::cleanPath(dir));
        reload();
    });
    connect(open, &QPushButton::clicked, this, [this, key] {
        const QString dir = key == "folders/covers" ? m_library.coversDir() : m_library.setting(key).toString();
        if (!dir.isEmpty())
        {
            QDir().mkpath(dir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        }
    });
    connect(reset, &QPushButton::clicked, this, [this, key] {
        changed(key, QString());
        reload();
    });
    for (QWidget *x : {static_cast<QWidget *>(path), static_cast<QWidget *>(browse), static_cast<QWidget *>(reset)})
        help(x, title, description + " Reset goes back to: " + defaultText + ".");
    row->addWidget(path, 1);
    row->addWidget(browse);
    row->addWidget(open);
    row->addWidget(reset);
    bl->addLayout(row);
    return box;
}

QWidget *SettingsDialog::buildFoldersPage()
{
    QVBoxLayout *l;
    QWidget *w = page(&l);
    l->addWidget(folderRow("Covers Directory", "Used for the covers in the game grid, named by game ID (e.g. SBKX.jpg).",
                           "folders/covers", QDir::toNativeSeparators(m_library.appDir() + "/covers")));
    l->addWidget(folderRow("Save Data Directory",
                           "Used for each game's save data (sram.bin and eeprom.bin), in a folder per game.",
                           "folders/saves", "in each game's own folder"));
    l->addWidget(folderRow("Card Files Directory",
                           "Used for each game's player cards (Virtua Tennis 3, R-Tuned, Initial D), in a folder per game.",
                           "folders/cards", "in each game's own folder"));
    l->addStretch();
    return w;
}
