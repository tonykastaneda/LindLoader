#include "ControllerSettingsDialog.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "BindingButton.h"
#include "Bindings.h"
#include "GameLibrary.h"
#include "Gamepads.h"
#include "RawMice.h"

namespace
{
QLabel *description(const QString &text)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}
} // namespace

ControllerSettingsDialog::ControllerSettingsDialog(GameLibrary &library, Page page, QWidget *parent)
    : SettingsWindow(library, "Lindy Controller Settings", parent)
{
    addPage("settings", "Global Settings",
            "Where controller input comes from, for every game. Changes take effect the next time a game "
            "starts.\n\nMouse over an option for more information.",
            buildGlobalPage());
    addPage("crosshair", "Lightgun",
            "Mice and crosshairs for the lightgun games: House of the Dead 4, Ghost Squad Evolution, Let's Go Jungle, "
            "Rambo, Primeval Hunt and the rest. Changes take effect the next time a game starts.",
            buildLightgunPage());
    addPage("keyboard", "Hotkeys",
            "Keys and buttons that work in every game. Click a box, then press a key or controller button; "
            "right-click a box to clear it.",
            buildHotkeysPage());
    setDefaultsGroups({"input", "crosshairs", "hotkeys"});
    showPage(page);
}

QWidget *ControllerSettingsDialog::buildGlobalPage()
{
    auto *w = new QWidget;
    auto *columns = new QHBoxLayout(w);
    columns->setContentsMargins(4, 0, 8, 0);
    auto *l = new QVBoxLayout;

    auto *sdl = new QGroupBox("SDL Input Source");
    auto *sg = new QGridLayout(sdl);
    sg->addWidget(description("Games read controllers through SDL, which supports most of them, with vibration and "
                              "the extra features of DualShock 4 and DualSense pads."),
                  0, 0, 1, 2);
    sg->addWidget(check("DualShock 4 / DualSense Enhanced Mode", "input/enhancedMode",
                        "Reads DualShock 4 and DualSense pads in their full mode, for vibration and the light bar "
                        "over Bluetooth. Turn it off if such a pad misbehaves with other programs afterwards."),
                  1, 0);
    sg->addWidget(check("Enable SDL Raw Input", "input/sdlRawInput",
                        "Reads controllers through Windows raw input. It can help with some Xbox pads and adapters, "
                        "but leave it off unless one isn't working."),
                  1, 1);
    sg->addWidget(check("Enable HIDAPI Drivers", "input/hidapi",
                        "SDL's own drivers for Nintendo, PlayStation, Xbox and other USB and Bluetooth pads. Turn it "
                        "off to read them through Windows instead."),
                  2, 0);
    l->addWidget(sdl);

    auto *xinput = new QGroupBox("XInput Source");
    auto *xl = new QVBoxLayout(xinput);
    xl->addWidget(description("XInput supports Xbox 360, Xbox One and Xbox Series controllers, and other controllers "
                              "that work like them."));
    xl->addWidget(check("Enable XInput Input Source", "input/xinput",
                        "Reads Xbox controllers through XInput. Turn it off only if a controller shows up twice."));
    l->addWidget(xinput);

    auto *dinput = new QGroupBox("DInput Source");
    auto *dl = new QVBoxLayout(dinput);
    dl->addWidget(description("DirectInput supports older controllers that don't support XInput, such as many arcade "
                              "sticks, wheels and adapters."));
    dl->addWidget(check("Enable DInput Input Source", "input/directinput",
                        "Reads DirectInput controllers. Turn it off only if a controller shows up twice."));
    l->addWidget(dinput);

    l->addStretch();
    columns->addLayout(l, 1);

    // Detected Devices, like PCSX2's.
    auto *devicesBox = new QGroupBox("Detected Devices");
    auto *dv = new QVBoxLayout(devicesBox);
    auto *devices = new QListWidget;
    dv->addWidget(devices);
    devicesBox->setFixedWidth(260);
    auto refresh = [devices] {
        devices->clear();
        devices->addItem("Keyboard");
        const QList<RawMice::Mouse> mice = RawMice::list();
        if (mice.isEmpty())
            devices->addItem("Mouse");
        for (const RawMice::Mouse &m : mice)
            devices->addItem("Mouse: " + m.name);
        for (const Gamepads::Device &d : Gamepads::instance().devices())
            devices->addItem(d.name);
    };
    refresh();
    connect(&Gamepads::instance(), &Gamepads::devicesChanged, devices, refresh);
    columns->addWidget(devicesBox);
    return w;
}

QWidget *ControllerSettingsDialog::buildLightgunPage()
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(4, 0, 8, 0);
    auto *raw = new QGroupBox("Raw Input Source (Mice)");
    auto *rl = new QVBoxLayout(raw);
    rl->addWidget(description("Raw input tells mice apart, so in lightgun games every player can aim with their own "
                              "mouse instead of sharing the Windows pointer. Pick each player's mouse on the game's "
                              "Controls page."));
    rl->addWidget(check("Enable Raw Input for Mice", "input/rawMice",
                        "Lightgun games read each mouse on its own: player 1 aims with their mouse, player 2 with "
                        "theirs. The Windows pointer is hidden while the game runs."));
    l->addWidget(raw);

    // Crosshairs: the loader draws one per player in House of the Dead 4, Rambo, Ghost Squad Evolution and
    // Primeval Hunt, where a real lightgun has no pointer on screen.
    auto *cross = new QGroupBox("Crosshairs (Lightgun Games)");
    auto *cg = new QGridLayout(cross);
    cg->setColumnStretch(2, 1);
    cg->addWidget(description("Draws a crosshair where each player aims, for mice and controllers. Works in House of "
                              "the Dead 4, Rambo, Ghost Squad Evolution and Primeval Hunt."),
                  0, 0, 1, 5);
    cg->addWidget(check("Show Crosshairs", "crosshairs/enabled",
                        "Draws each player's crosshair over the game. Off: aim without one, as with a real lightgun."),
                  1, 0, 1, 5);
    for (int p = 1; p <= 2; ++p)
    {
        const QString key = QStringLiteral("crosshairs/p%1").arg(p);
        auto *preview = new QLabel;
        preview->setFixedSize(40, 40);
        preview->setAlignment(Qt::AlignCenter);
        auto *name = new QLabel;
        auto *browse = new QPushButton("Browse...");
        auto *reset = new QPushButton("Reset");
        onLoad([this, p, preview, name, key] {
            const QString file = m_library.crosshairPath(p);
            preview->setPixmap(QPixmap(file).scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            name->setText(m_library.setting(key).toString().isEmpty() ? QStringLiteral("Default (%1)").arg(p == 1 ? "red" : "blue")
                                                                      : QFileInfo(file).fileName());
            name->setToolTip(QDir::toNativeSeparators(file));
        });
        connect(browse, &QPushButton::clicked, this, [this, p, key] {
            const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Player %1 Crosshair").arg(p), QString(),
                                                              "PNG images (*.png)");
            if (file.isEmpty())
                return;
            changed(key, QDir::cleanPath(file));
            reload();
        });
        connect(reset, &QPushButton::clicked, this, [this, key] {
            changed(key, QString());
            reload();
        });
        const QString helpText = "A PNG image, ideally square with a transparent background. Reset goes back to "
                                 "Lindy's own crosshair.";
        help(browse, QStringLiteral("Player %1 Crosshair").arg(p), helpText);
        cg->addWidget(new QLabel(QStringLiteral("Player %1:").arg(p)), 1 + p, 0);
        cg->addWidget(preview, 1 + p, 1);
        cg->addWidget(name, 1 + p, 2);
        cg->addWidget(browse, 1 + p, 3);
        cg->addWidget(reset, 1 + p, 4);
    }
    auto *size = new QSpinBox;
    size->setRange(16, 256);
    size->setSuffix(" px");
    onLoad([this, size] { size->setValue(m_library.setting("crosshairs/size").toInt()); });
    connect(size, &QSpinBox::valueChanged, this, [this](int v) { changed("crosshairs/size", v); });
    help(size, "Crosshair Size", "How big the crosshairs are drawn, in pixels of the game's picture.");
    cg->addWidget(new QLabel("Size:"), 4, 0);
    cg->addWidget(size, 4, 1, 1, 2, Qt::AlignLeft);
    l->addWidget(cross);
    l->addStretch();
    return w;
}

QWidget *ControllerSettingsDialog::buildHotkeysPage()
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(4, 0, 8, 0);
    QString section;
    QGridLayout *grid = nullptr;
    for (const AppSettings::Hotkey &h : AppSettings::hotkeys())
    {
        if (section != h.section)
        {
            section = h.section;
            auto *heading = new QLabel(section);
            heading->setStyleSheet("font-size: 14pt;");
            auto *line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            l->addWidget(heading);
            l->addWidget(line);
            grid = new QGridLayout;
            grid->setColumnStretch(0, 1);
            l->addLayout(grid);
        }
        const QString key = QString("hotkeys/") + h.key;
        auto *button = new BindingButton(ControlLayouts::Binding{h.key, "Common", false}, 1);
        button->setMinimumWidth(360);
        button->bound = [this, key](BindingButton *b, const QString &binding, const QString &) {
            changed(key, Bindings::replace(m_library.setting(key).toString(), binding, 0));
            b->setBindings(m_library.setting(key).toString());
        };
        button->cleared = [this, key](BindingButton *b) {
            changed(key, QString());
            b->setBindings(QString());
        };
        onLoad([this, button, key] { button->setBindings(m_library.setting(key).toString()); });
        const int row = grid->rowCount();
        auto *label = new QLabel(h.title);
        grid->addWidget(label, row, 0);
        grid->addWidget(button, row, 1);
        help(button, h.title, "Click, then press a key or controller button. Right-click to clear.");
        m_hotkeys << button;
    }
    auto *note = description("Without a binding, Toggle Fullscreen is Alt+Enter or F11 and Toggle FPS Overlay is F12.");
    note->setStyleSheet("color: palette(placeholder-text);");
    l->addSpacing(12);
    l->addWidget(note);
    l->addStretch();
    return w;
}
