#include "ControllerSettingsWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

#include "Bindings.h"
#include "Gamepads.h"

using ControlLayouts::Binding;
using ControlLayouts::Group;
using ControlLayouts::Layout;

namespace
{
const int kMaxPlayers = 4;
const int kCaptureSeconds = 5;
const int kAxisMax = 32767;
} // namespace

// One binding: shows what the action is bound to; click to bind, right-click to clear.
class BindingButton : public QPushButton
{
public:
    BindingButton(const Binding &binding, int player, QWidget *parent = nullptr)
        : QPushButton(parent), binding(binding), player(player)
    {
        setMinimumWidth(150);
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

    const Binding binding;
    const int player; // 1 or 2
    std::function<void(BindingButton *, const QString &, const QString &)> bound;
    std::function<void(BindingButton *)> cleared;

protected:
    bool event(QEvent *e) override
    {
        if (m_capturing && (e->type() == QEvent::KeyPress || e->type() == QEvent::ShortcutOverride))
        {
            auto *key = static_cast<QKeyEvent *>(e);
            if (e->type() == QEvent::KeyPress && !key->isAutoRepeat() && !binding.analog)
            {
                const QString b = Bindings::fromKeyEvent(key);
                if (!b.isEmpty())
                {
                    stopCapture();
                    bound(this, b, QString());
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
        setText(QStringLiteral("%1... %2").arg(binding.analog ? "Move a stick or trigger" : "Press a key or button").arg(m_seconds));
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
                               [this](const QString &b, const QString &guid) {
                                   stopCapture();
                                   bound(this, b, guid);
                               });
        Gamepads::instance().startCapture(binding.analog);
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
    : QWidget(parent), m_game(game), m_library(library), m_layout(ControlLayouts::forType(game.type))
{
    m_ini.load(m_game.controlsPath());
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    if (!m_layout)
    {
        auto *text = new QLabel("This game uses Linux Loader's default controls. A controls page for it isn't "
                                "available yet; its bindings can be changed in its controls.ini.");
        text->setWordWrap(true);
        auto *open = new QPushButton("Open controls.ini");
        connect(open, &QPushButton::clicked, this,
                [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_game.controlsPath())); });
        root->addWidget(text);
        root->addWidget(open, 0, Qt::AlignLeft);
        root->addStretch();
        return;
    }

    // Bindings | Settings, like PCSX2x6.
    auto *views = new QHBoxLayout;
    auto *stack = new QStackedWidget;
    const struct
    {
        const char *icon, *text;
    } tabs[] = {{":/icons/gamepad.svg", "Bindings"}, {":/icons/settings.svg", "Settings"}};
    for (int i = 0; i < 2; ++i)
    {
        auto *tab = new QToolButton;
        tab->setIcon(QIcon(tabs[i].icon));
        tab->setText(tabs[i].text);
        tab->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tab->setCheckable(true);
        tab->setAutoExclusive(true);
        tab->setChecked(i == 0);
        connect(tab, &QToolButton::toggled, stack, [stack, i](bool on) {
            if (on)
                stack->setCurrentIndex(i);
        });
        views->addWidget(tab);
    }
    views->addStretch();
    views->addWidget(new QLabel(QStringLiteral("Layout: <b>%1</b>").arg(m_layout->name)));
    root->addLayout(views);

    stack->addWidget(buildBindingsPage());
    stack->addWidget(buildSettingsPage());
    root->addWidget(stack, 1);

    auto *hint = new QLabel("Click a box, then press a key or controller button. Right-click a box to clear it.");
    hint->setStyleSheet("color: palette(placeholder-text);");
    auto *copy = new QPushButton("Copy From...");
    connect(copy, &QPushButton::clicked, this, &ControllerSettingsWidget::copyFrom);
    auto *reset = new QPushButton("Reset to Defaults");
    connect(reset, &QPushButton::clicked, this, &ControllerSettingsWidget::resetToDefaults);
    auto *footer = new QHBoxLayout;
    footer->addWidget(hint, 1);
    footer->addWidget(copy);
    footer->addWidget(reset);
    root->addLayout(footer);

    connect(&Gamepads::instance(), &Gamepads::devicesChanged, this, &ControllerSettingsWidget::refreshControllers);
    refresh();
    refreshControllers();
}

QWidget *ControllerSettingsWidget::buildBindingsPage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(0, 0, 0, 0);

    // Which controller each player uses, and Automap.
    auto *devices = new QGroupBox("Controllers");
    auto *dg = new QGridLayout(devices);
    dg->setColumnStretch(1, 1);
    for (int p = 1; p <= (m_layout->twoPlayers ? 2 : 1); ++p)
    {
        auto *combo = new QComboBox;
        connect(combo, &QComboBox::activated, this, [this, p](int index) { selectController(p, index); });
        auto *automapButton = new QPushButton("Automap");
        automapButton->setToolTip("Bind this player's controls to the standard layout on this controller");
        connect(automapButton, &QPushButton::clicked, this, [this, p] { automap(p); });
        dg->addWidget(new QLabel(QStringLiteral("Player %1:").arg(p)), p - 1, 0);
        dg->addWidget(combo, p - 1, 1);
        dg->addWidget(automapButton, p - 1, 2);
        m_controllers << combo;
    }
    l->addWidget(devices);

    // System Buttons on the left, the layout's groups on the right; any further groups go left.
    auto *columns = new QHBoxLayout;
    auto *left = new QVBoxLayout, *right = new QVBoxLayout;
    left->addWidget(buildGroup(ControlLayouts::systemButtons(*m_layout)));
    for (int i = 0; i < m_layout->groups.size(); ++i)
        (i < 2 ? right : left)->addWidget(buildGroup(m_layout->groups[i]));
    left->addStretch();
    right->addStretch();
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    l->addLayout(columns, 1);
    return page;
}

QWidget *ControllerSettingsWidget::buildGroup(const Group &group)
{
    auto *box = new QGroupBox(group.title);
    auto *grid = new QGridLayout(box);
    grid->setColumnStretch(1, 1);
    int row = 0;
    if (m_layout->twoPlayers)
    {
        grid->setColumnStretch(2, 1);
        grid->addWidget(new QLabel("P1"), row, 1, Qt::AlignHCenter);
        grid->addWidget(new QLabel("P2"), row, 2, Qt::AlignHCenter);
        ++row;
    }
    for (const ControlLayouts::Row &r : group.rows)
    {
        grid->addWidget(new QLabel(r.label), row, 0);
        const Binding *columns[] = {&r.p1, &r.p2};
        for (int p = 1; p <= 2; ++p)
        {
            if (columns[p - 1]->key.isEmpty())
                continue;
            auto *button = new BindingButton(*columns[p - 1], p);
            button->bound = [this](BindingButton *b, const QString &binding, const QString &guid) { bind(b, binding, guid); };
            button->cleared = [this](BindingButton *b) { clear(b); };
            m_buttons << button;
            grid->addWidget(button, row, p);
        }
        ++row;
    }
    return box;
}

QWidget *ControllerSettingsWidget::buildSettingsPage()
{
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(0, 0, 0, 0);
    auto *box = new QGroupBox(m_layout->name);
    auto *form = new QGridLayout(box);
    form->setColumnStretch(0, 1);
    int row = 0;
    for (const ControlLayouts::Setting &s : m_layout->settings)
    {
        const QStringList keys = s.key.split(',');
        const QString value = m_ini.value("Config", keys.first(), s.fallback);
        if (s.kind == ControlLayouts::Setting::Percent)
        {
            auto *spin = new QSpinBox;
            spin->setRange(0, 100);
            spin->setSuffix(" %");
            spin->setValue(qRound(value.toDouble() * 100.0 / kAxisMax));
            connect(spin, &QSpinBox::valueChanged, this, [this, keys](int v) {
                for (const QString &k : keys)
                    m_ini.setValue("Config", k, QString::number(v * kAxisMax / 100));
                save();
            });
            form->addWidget(new QLabel(s.label), row, 0);
            form->addWidget(spin, row++, 1);
        }
        else
        {
            auto *check = new QCheckBox(s.label);
            check->setChecked(value.trimmed() == "1");
            connect(check, &QCheckBox::toggled, this, [this, keys](bool on) {
                for (const QString &k : keys)
                    m_ini.setValue("Config", k, on ? "1" : "0");
                save();
            });
            form->addWidget(check, row++, 0, 1, 2);
        }
    }
    l->addWidget(box);

    auto *open = new QPushButton("Open controls.ini");
    connect(open, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_game.controlsPath())); });
    l->addWidget(open, 0, Qt::AlignLeft);
    l->addStretch();
    return page;
}

void ControllerSettingsWidget::refresh()
{
    for (BindingButton *b : m_buttons)
        b->setBindings(m_ini.value(b->binding.section, b->binding.key, QString()));
}

void ControllerSettingsWidget::refreshControllers()
{
    for (int p = 1; p <= m_controllers.size(); ++p)
    {
        QComboBox *combo = m_controllers[p - 1];
        const QString saved = m_ini.value("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(p), QString()).remove('"');
        combo->clear();
        combo->addItem(QStringLiteral("Automatic (controller %1)").arg(p), QString());
        bool found = false;
        for (const Gamepads::Device &d : Gamepads::instance().devices())
        {
            combo->addItem(d.name, d.guid);
            found |= d.guid == saved;
        }
        if (!saved.isEmpty() && !found)
            combo->addItem("Saved controller (not connected)", saved);
        const int i = combo->findData(saved);
        combo->setCurrentIndex(i < 0 ? 0 : i);
    }
}

void ControllerSettingsWidget::selectController(int player, int index)
{
    const QString guid = m_controllers[player - 1]->itemData(index).toString();
    const QString key = QStringLiteral("P%1_GUID").arg(player);
    if (guid.isEmpty())
        m_ini.removeValue("ControllerGUIDs", key);
    else
        m_ini.setValue("ControllerGUIDs", key, guid);
    save();
}

void ControllerSettingsWidget::automap(int player)
{
    ControlLayouts::automap(m_ini, *m_layout, player);
    save();
    refresh();
}

void ControllerSettingsWidget::bind(BindingButton *button, const QString &binding, const QString &guid)
{
    const Binding &b = button->binding;
    m_ini.setValue(b.section, b.key, Bindings::replace(m_ini.value(b.section, b.key, QString()), binding, button->player - 1));

    // Binding with a controller makes it this player's controller, unless another player has it.
    if (!guid.isEmpty())
    {
        bool taken = false;
        for (int p = 1; p <= kMaxPlayers; ++p)
            taken |= p != button->player &&
                     m_ini.value("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(p), QString()) == guid;
        if (!taken)
            m_ini.setValue("ControllerGUIDs", QStringLiteral("P%1_GUID").arg(button->player), guid);
    }
    save();
    refresh();
    refreshControllers();
}

void ControllerSettingsWidget::clear(BindingButton *button)
{
    m_ini.setValue(button->binding.section, button->binding.key, QString());
    save();
    refresh();
}

void ControllerSettingsWidget::copyFrom()
{
    QStringList names;
    QList<const Game *> sources;
    for (const Game &g : m_library.games())
        if (g.type == m_game.type && g.gameDir != m_game.gameDir && QFileInfo::exists(g.controlsPath()))
        {
            names << g.title;
            sources << &g;
        }
    if (names.isEmpty())
    {
        QMessageBox::information(this, "Copy From",
                                 QStringLiteral("No other %1 game in your library has controls to copy yet.").arg(m_layout->name));
        return;
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, "Copy From", "Copy the controls of:", names, 0, false, &ok);
    if (!ok)
        return;
    const QString from = sources[names.indexOf(pick)]->controlsPath();
    QFile::remove(m_game.controlsPath());
    if (!QFile::copy(from, m_game.controlsPath()))
    {
        QMessageBox::warning(this, "LindLoader", "Could not copy the controls.");
        return;
    }
    m_ini.load(m_game.controlsPath());
    refresh();
    refreshControllers();
}

void ControllerSettingsWidget::resetToDefaults()
{
    if (QMessageBox::question(this, "Reset to Defaults", "Reset the controls of " + m_game.title + " to the defaults?") !=
        QMessageBox::Yes)
        return;
    QTemporaryDir dir;
    if (!dir.isValid() || !m_library.createDefaults("controls", dir.path(), "controls.ini"))
    {
        QMessageBox::warning(this, "LindLoader", "Could not create the default controls.");
        return;
    }
    m_library.applyStarterControls(m_game.type, dir.path() + "/controls.ini");
    QFile::remove(m_game.controlsPath());
    QFile::copy(dir.path() + "/controls.ini", m_game.controlsPath());
    m_ini.load(m_game.controlsPath());
    refresh();
    refreshControllers();
}

void ControllerSettingsWidget::save()
{
    if (!m_ini.save())
        QMessageBox::warning(this, "LindLoader", "Could not save " + QDir::toNativeSeparators(m_ini.path()));
}
