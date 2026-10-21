#include "ControllerSettingsWidget.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSvgWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

#include "Bindings.h"
#include "Gamepads.h"

struct BindingDef
{
    QString label;
    QString key;     // "%1" is the player number
    QString section; // empty: the game's section
    bool analog = false;
};

namespace
{
const int kMaxPlayers = 4;
const int kCaptureSeconds = 5;

struct Layout
{
    QString image;
    bool players = false; // has a Player 2
    QList<QPair<QString, QList<BindingDef>>> left, right;
    QList<BindingDef> system;
};

QList<BindingDef> systemBindings(bool card, const QString &cardKey)
{
    QList<BindingDef> s = {{"Start", "P%1_Start", "Common"},
                           {"Coin", "P%1_Coin", "Common"},
                           {"Service", "P%1_Service", "Common"}};
    if (card)
        s << BindingDef{"Card Insert", cardKey};
    s << BindingDef{"Test", "Test", "Common"} << BindingDef{"Exit Game", "ExitGame", "Common"};
    return s;
}

bool isAnalogKey(const QString &key)
{
    for (const char *a : {"Steer", "Gas", "Brake", "GunX", "GunY", "Flying_X", "Flying_Y", "Throttle"})
        if (key.endsWith(QLatin1String(a)))
            return true;
    return false;
}

Layout layoutFor(const QString &type, const IniFile &ini, const QString &section)
{
    Layout l;
    if (type == "digital")
    {
        l.image = ":/images/arcade.svg";
        l.players = true;
        l.left = {{"Joystick", {{"Up", "P%1_Up"}, {"Down", "P%1_Down"}, {"Left", "P%1_Left"}, {"Right", "P%1_Right"}}}};
        l.right = {{"Buttons", {{"Button 1", "P%1_Button1"}, {"Button 2", "P%1_Button2"}, {"Button 3", "P%1_Button3"}}}};
        l.system = systemBindings(true, "P%1_Card%1Insert");
        return l;
    }
    if (type == "driving")
    {
        l.image = ":/images/wheel.svg";
        l.left = {{"Steering",
                   {{"Steering", "P1_Steer", {}, true}, {"Steer Left", "P1_Steer_Left"}, {"Steer Right", "P1_Steer_Right"}}},
                  {"Pedals",
                   {{"Gas", "P1_Gas", {}, true},
                    {"Brake", "P1_Brake", {}, true},
                    {"Gas (button)", "P1_Gas_Digital"},
                    {"Brake (button)", "P1_Brake_Digital"}}}};
        l.right = {{"Buttons",
                    {{"View Change", "ViewChange"},
                     {"Boost", "Boost"},
                     {"Boost Right", "BoostRight"},
                     {"Gear Up", "GearUp"},
                     {"Gear Down", "GearDown"},
                     {"Music Change", "MusicChange"}}},
                   {"Menu", {{"Up", "Up"}, {"Down", "Down"}, {"Left", "Left"}, {"Right", "Right"}}}};
        l.system = systemBindings(true, "CardInsert");
        return l;
    }
    // Other game types: every action in the game's section, in two columns.
    l.image = ":/icons/gamepad.svg";
    const QStringList keys = ini.keys(section);
    QList<BindingDef> a, b;
    for (int i = 0; i < keys.size(); ++i)
        (i < (keys.size() + 1) / 2 ? a : b) << BindingDef{QString(keys[i]).replace('_', ' '), keys[i], {}, isAnalogKey(keys[i])};
    l.left = {{"Controls", a}};
    if (!b.isEmpty())
        l.right = {{"", b}};
    l.system = systemBindings(false, {});
    return l;
}
} // namespace

// One binding: shows what the action is bound to; click to bind, right-click to clear.
class BindingButton : public QPushButton
{
public:
    BindingButton(const BindingDef &def, QWidget *parent = nullptr) : QPushButton(parent), def(def)
    {
        setMinimumWidth(190);
        setMinimumHeight(28);
        connect(this, &QPushButton::clicked, this, &BindingButton::startCapture);
        m_countdown.setInterval(1000);
        connect(&m_countdown, &QTimer::timeout, this, [this] {
            if (--m_seconds <= 0)
                stopCapture();
            else
                showPrompt();
        });
    }
    ~BindingButton() override { stopCapture(); }

    void setBindings(const QString &value)
    {
        QStringList names;
        for (const QString &b : Bindings::split(value))
            names << Bindings::displayName(b);
        m_text = names.isEmpty() ? QStringLiteral("-") : names.join(", ");
        setToolTip(names.isEmpty() ? QStringLiteral("Not bound") : names.join("\n"));
        if (!m_capturing)
            showText();
    }

    BindingDef def;
    std::function<void(BindingButton *, const QString &, const QString &)> bound;
    std::function<void(BindingButton *)> cleared;

protected:
    bool event(QEvent *e) override
    {
        if (m_capturing && (e->type() == QEvent::KeyPress || e->type() == QEvent::ShortcutOverride))
        {
            auto *key = static_cast<QKeyEvent *>(e);
            if (e->type() == QEvent::KeyPress && !key->isAutoRepeat() && !def.analog)
            {
                const QString binding = Bindings::fromKeyEvent(key);
                if (!binding.isEmpty())
                {
                    stopCapture();
                    bound(this, binding, QString());
                }
            }
            e->accept();
            return true;
        }
        return QPushButton::event(e);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::RightButton)
        {
            stopCapture();
            cleared(this);
            return;
        }
        if (m_capturing)
            return; // a second click doesn't restart the countdown
        QPushButton::mousePressEvent(e);
    }

    void resizeEvent(QResizeEvent *e) override
    {
        QPushButton::resizeEvent(e);
        if (!m_capturing)
            showText();
    }

private:
    void showText() { setText(fontMetrics().elidedText(m_text, Qt::ElideRight, width() - 16)); }

    void showPrompt()
    {
        setText(QStringLiteral("%1... %2")
                    .arg(def.analog ? "Move a stick, trigger or pedal" : "Press a key or button")
                    .arg(m_seconds));
    }

    void startCapture()
    {
        if (s_active && s_active != this)
            s_active->stopCapture();
        s_active = this;
        m_capturing = true;
        m_seconds = kCaptureSeconds;
        showPrompt();
        setDown(true);
        grabKeyboard();
        m_countdown.start();
        m_connection = connect(&Gamepads::instance(), &Gamepads::captured, this,
                               [this](const QString &binding, const QString &guid) {
                                   stopCapture();
                                   bound(this, binding, guid);
                               });
        Gamepads::instance().startCapture(def.analog);
    }

    void stopCapture()
    {
        if (!m_capturing)
            return;
        m_capturing = false;
        if (s_active == this)
            s_active = nullptr;
        Gamepads::instance().stopCapture();
        disconnect(m_connection);
        m_countdown.stop();
        releaseKeyboard();
        setDown(false);
        showText();
    }

    static inline BindingButton *s_active = nullptr;
    QString m_text;
    bool m_capturing = false;
    int m_seconds = 0;
    QTimer m_countdown;
    QMetaObject::Connection m_connection;
};

ControllerSettingsWidget::ControllerSettingsWidget(const Game &game, GameLibrary &library, QWidget *parent)
    : QWidget(parent), m_game(game), m_library(library)
{
    m_ini.load(m_game.controlsPath());
    const QString type = m_game.type.isEmpty() ? QStringLiteral("digital") : m_game.type;
    m_section = type.left(1).toUpper() + type.mid(1);
    const Layout layout = layoutFor(type, m_ini, m_section);

    // Player and controller selection.
    auto *top = new QHBoxLayout;
    if (layout.players)
    {
        m_players = new QComboBox;
        m_players->addItems({"Player 1", "Player 2"});
        connect(m_players, &QComboBox::currentIndexChanged, this, [this](int i) {
            m_player = i + 1;
            refresh();
            refreshControllers();
        });
        top->addWidget(new QLabel("Player:"));
        top->addWidget(m_players);
        top->addSpacing(16);
    }
    m_controllers = new QComboBox;
    m_controllers->setMinimumWidth(320);
    connect(m_controllers, &QComboBox::activated, this, &ControllerSettingsWidget::selectController);
    top->addWidget(new QLabel("Controller:"));
    top->addWidget(m_controllers);
    top->addStretch();

    // Binding groups around the picture.
    auto *columns = new QHBoxLayout;
    auto *left = new QVBoxLayout, *right = new QVBoxLayout;
    for (const auto &g : layout.left)
        left->addWidget(buildGroup(g.first, g.second));
    for (const auto &g : layout.right)
        right->addWidget(buildGroup(g.first, g.second));
    left->addStretch();
    right->addStretch();
    auto *picture = new QSvgWidget(layout.image);
    picture->setFixedSize(320, 200);
    auto *center = new QVBoxLayout;
    center->addStretch();
    center->addWidget(picture, 0, Qt::AlignHCenter);
    center->addStretch();
    columns->addLayout(left, 1);
    columns->addLayout(center);
    columns->addLayout(right, 1);

    // System buttons underneath, three to a row.
    auto *system = new QGroupBox("System");
    auto *sg = new QGridLayout(system);
    for (int i = 0; i < layout.system.size(); ++i)
    {
        auto *button = new BindingButton(layout.system[i]);
        button->bound = [this](BindingButton *b, const QString &binding, const QString &guid) { bind(b, binding, guid); };
        button->cleared = [this](BindingButton *b) { clear(b); };
        m_buttons << button;
        sg->addWidget(new QLabel(layout.system[i].label + ":"), i / 3, (i % 3) * 2);
        sg->addWidget(button, i / 3, (i % 3) * 2 + 1);
        sg->setColumnStretch((i % 3) * 2 + 1, 1);
    }

    auto *hint = new QLabel("Click a binding, then press a key or controller button. Right-click a binding to clear it.");
    hint->setStyleSheet("color: palette(placeholder-text);");
    auto *reset = new QPushButton("Reset to Defaults");
    connect(reset, &QPushButton::clicked, this, &ControllerSettingsWidget::resetToDefaults);
    auto *open = new QPushButton("Open controls.ini");
    connect(open, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_game.controlsPath())); });
    auto *footer = new QHBoxLayout;
    footer->addWidget(hint, 1);
    footer->addWidget(reset);
    footer->addWidget(open);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addLayout(top);
    root->addLayout(columns, 1);
    root->addWidget(system);
    root->addLayout(footer);

    connect(&Gamepads::instance(), &Gamepads::devicesChanged, this, &ControllerSettingsWidget::refreshControllers);
    refresh();
    refreshControllers();
}

QWidget *ControllerSettingsWidget::buildGroup(const QString &title, const QList<BindingDef> &binds)
{
    auto *box = new QGroupBox(title);
    auto *grid = new QGridLayout(box);
    grid->setColumnStretch(1, 1);
    for (int i = 0; i < binds.size(); ++i)
    {
        auto *button = new BindingButton(binds[i]);
        button->bound = [this](BindingButton *b, const QString &binding, const QString &guid) { bind(b, binding, guid); };
        button->cleared = [this](BindingButton *b) { clear(b); };
        m_buttons << button;
        grid->addWidget(new QLabel(binds[i].label + ":"), i, 0);
        grid->addWidget(button, i, 1);
    }
    return box;
}

QString ControllerSettingsWidget::keyFor(const BindingButton *button) const
{
    return button->def.key.contains("%1") ? button->def.key.arg(m_player) : button->def.key;
}

void ControllerSettingsWidget::refresh()
{
    for (BindingButton *b : m_buttons)
        b->setBindings(m_ini.value(b->def.section.isEmpty() ? m_section : b->def.section, keyFor(b), QString()));
}

void ControllerSettingsWidget::refreshControllers()
{
    const QString key = QStringLiteral("P%1_GUID").arg(m_player);
    const QString saved = m_ini.value("ControllerGUIDs", key, QString()).remove('"');
    m_controllers->clear();
    m_controllers->addItem("Automatic (first free controller)", QString());
    bool found = false;
    for (const Gamepads::Device &d : Gamepads::instance().devices())
    {
        m_controllers->addItem(d.name, d.guid);
        found |= d.guid == saved;
    }
    if (!saved.isEmpty() && !found)
        m_controllers->addItem("Saved controller (not connected)", saved);
    const int i = m_controllers->findData(saved);
    m_controllers->setCurrentIndex(i < 0 ? 0 : i);
}

void ControllerSettingsWidget::selectController(int index)
{
    const QString guid = m_controllers->itemData(index).toString();
    const QString key = QStringLiteral("P%1_GUID").arg(m_player);
    if (guid.isEmpty())
        m_ini.removeValue("ControllerGUIDs", key);
    else
        m_ini.setValue("ControllerGUIDs", key, guid);
    save();
}

void ControllerSettingsWidget::bind(BindingButton *button, const QString &binding, const QString &guid)
{
    const QString section = button->def.section.isEmpty() ? m_section : button->def.section;
    const QString key = keyFor(button);
    m_ini.setValue(section, key, Bindings::replace(m_ini.value(section, key, QString()), binding, m_player - 1));

    // Binding with a controller makes it this player's controller, unless another player has it.
    if (!guid.isEmpty())
    {
        bool taken = false;
        for (int p = 1; p <= kMaxPlayers; ++p)
            taken |= p != m_player && m_ini.value("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(p), QString()) == guid;
        if (!taken)
            m_ini.setValue("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(m_player), guid);
    }
    save();
    refresh();
    refreshControllers();
}

void ControllerSettingsWidget::clear(BindingButton *button)
{
    m_ini.setValue(button->def.section.isEmpty() ? m_section : button->def.section, keyFor(button), QString());
    save();
    refresh();
}

void ControllerSettingsWidget::resetToDefaults()
{
    if (QMessageBox::question(this, "Reset to Defaults",
                              "Reset the controls of " + m_game.title + " to the Linux Loader defaults?") != QMessageBox::Yes)
        return;
    QTemporaryDir dir;
    IniFile defaults;
    if (!dir.isValid() || !m_library.createDefaults("controls", dir.path(), "controls.ini") ||
        !defaults.load(dir.path() + "/controls.ini"))
    {
        QMessageBox::warning(this, "LindLoader", "Could not create the default controls.");
        return;
    }
    for (const QString &section : {QStringLiteral("Common"), m_section})
    {
        for (const QString &key : m_ini.keys(section))
            if (!defaults.keys(section).contains(key))
                m_ini.removeValue(section, key);
        for (const QString &key : defaults.keys(section))
            m_ini.setValue(section, key, defaults.value(section, key, QString()));
    }
    save();
    refresh();
}

void ControllerSettingsWidget::save()
{
    if (!m_ini.save())
        QMessageBox::warning(this, "LindLoader", "Could not save " + QDir::toNativeSeparators(m_ini.path()));
}
