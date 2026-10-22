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
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

#include "BindingButton.h"
#include "Bindings.h"
#include "Gamepads.h"
#include "RawMice.h"
#include "Theme.h"

using ControlLayouts::Binding;
using ControlLayouts::Group;
using ControlLayouts::Layout;

namespace
{
const int kMaxPlayers = 4;
const int kAxisMax = 32767;
} // namespace

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
    } tabs[] = {{"gamepad", "Bindings"}, {"settings", "Settings"}};
    for (int i = 0; i < 2; ++i)
    {
        auto *tab = new QToolButton;
        tab->setIcon(Theme::icon(tabs[i].icon));
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
    refreshMice();
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

    // Lightgun games: each player can aim with their own mouse (Controller Settings > Lightgun).
    if (m_layout->name == "Lightgun")
    {
        auto *mice = new QGroupBox("Mice");
        auto *mg = new QGridLayout(mice);
        mg->setColumnStretch(1, 1);
        for (int p = 1; p <= 2; ++p)
        {
            auto *combo = new QComboBox;
            // Any change, by mouse, keyboard or accessibility tools; refreshMice() fills the list quietly.
            connect(combo, &QComboBox::currentIndexChanged, this, [this, p, combo](int index) {
                if (!m_refreshingMice && index >= 0)
                    selectMouse(p, combo->itemData(index).toString());
            });
            auto *identify = new QPushButton("Identify...");
            identify->setToolTip("Click a button on this player's mouse to pick it");
            connect(identify, &QPushButton::clicked, this, [this, p] {
                MouseIdentifyDialog dialog(QStringLiteral("Click a button on <b>Player %1</b>'s mouse.").arg(p), this);
                if (dialog.exec() == QDialog::Accepted)
                    selectMouse(p, dialog.path());
            });
            mg->addWidget(new QLabel(QStringLiteral("Player %1:").arg(p)), p - 1, 0);
            mg->addWidget(combo, p - 1, 1);
            mg->addWidget(identify, p - 1, 2);
            m_mice << combo;
        }
        auto *note = new QLabel("Picking a player's own mouse turns on <b>Raw Input</b> (Controller Settings > Lightgun). "
                                "\"Any mouse\" moves the Windows pointer, shared by every mouse.");
        note->setWordWrap(true);
        note->setStyleSheet("color: palette(placeholder-text);");
        mg->addWidget(note, 2, 0, 1, 3);
        l->addWidget(mice);
    }

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

void ControllerSettingsWidget::refreshMice()
{
    const QList<RawMice::Mouse> mice = RawMice::list();
    const QScopedValueRollback<bool> quiet(m_refreshingMice, true);
    for (int p = 1; p <= m_mice.size(); ++p)
    {
        QComboBox *combo = m_mice[p - 1];
        const QString saved = m_ini.value("MouseDevices", QStringLiteral("MOUSE%1").arg(p), QString()).remove('"');
        combo->clear();
        combo->addItem("Any mouse", QString());
        bool found = false;
        for (int i = 0; i < mice.size(); ++i)
        {
            // Two of the same mouse get numbered.
            int same = 0;
            for (const RawMice::Mouse &m : mice)
                same += m.name == mice[i].name;
            combo->addItem(same > 1 ? QStringLiteral("%1 (%2)").arg(mice[i].name).arg(i + 1) : mice[i].name, mice[i].path);
            found |= mice[i].path.compare(saved, Qt::CaseInsensitive) == 0;
        }
        if (!saved.isEmpty() && !found)
            combo->addItem("Saved mouse (not connected)", saved);
        int index = 0;
        for (int i = 1; i < combo->count(); ++i)
            if (combo->itemData(i).toString().compare(saved, Qt::CaseInsensitive) == 0)
                index = i;
        combo->setCurrentIndex(index);
    }
}

// Ties a player's mouse bindings to their own mouse (MOUSEn_, n = player) or back to any mouse (MOUSE_).
void ControllerSettingsWidget::selectMouse(int player, const QString &path)
{
    const QString key = QStringLiteral("MOUSE%1").arg(player);
    const QString prefix = path.isEmpty() ? QStringLiteral("MOUSE_") : QStringLiteral("MOUSE%1_").arg(player);
    if (path.isEmpty())
        m_ini.removeValue("MouseDevices", key);
    else
        m_ini.setValue("MouseDevices", key, "\"" + path + "\"");
    // A player's own mouse only works with raw input: turn it on rather than leave the choice dead.
    if (!path.isEmpty() && !m_library.setting("input/rawMice").toBool())
        m_library.setSetting("input/rawMice", true);

    static const QRegularExpression anyMouse("^MOUSE\\d?_");
    bool hasMouse = false;
    for (const Group &g : m_layout->groups)
        for (const ControlLayouts::Row &r : g.rows)
        {
            const Binding &b = player == 1 ? r.p1 : r.p2;
            if (b.key.isEmpty())
                continue;
            QStringList parts = Bindings::split(m_ini.value(b.section, b.key, QString()));
            for (QString &part : parts)
                if (part.contains(anyMouse))
                {
                    part.replace(anyMouse, prefix);
                    hasMouse = true;
                }
            m_ini.setValue(b.section, b.key, parts.join(", "));
        }

    // A player without mouse bindings yet gets the loader's usual ones (as player 1 has by default).
    if (!path.isEmpty() && !hasMouse)
    {
        const QString s = "Shooting", p = QString::number(player);
        const QPair<QString, QString> standard[] = {{"P" + p + "_GunX", "AXIS_X"},
                                                    {"P" + p + "_GunY", "AXIS_Y"},
                                                    {"P" + p + "_Trigger", "LEFT_BUTTON"},
                                                    {"P" + p + "_Reload", "RIGHT_BUTTON"},
                                                    {"P" + p + "_GunButton", "MIDDLE_BUTTON"}};
        for (const auto &[k, control] : standard)
            m_ini.setValue(s, k, Bindings::replace(m_ini.value(s, k, QString()), prefix + control, player - 1));
    }
    save();
    refresh();
    refreshMice();
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
    // A mouse button of a player who has their own mouse is that mouse's button.
    QString added = binding;
    if (added.startsWith("MOUSE_") &&
        !m_ini.value("MouseDevices", QStringLiteral("MOUSE%1").arg(button->player), QString()).isEmpty())
        added.replace(0, 6, QStringLiteral("MOUSE%1_").arg(button->player));
    m_ini.setValue(b.section, b.key, Bindings::replace(m_ini.value(b.section, b.key, QString()), added, button->player - 1));

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
        QMessageBox::warning(this, "Lindy", "Could not copy the controls.");
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
        QMessageBox::warning(this, "Lindy", "Could not create the default controls.");
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
        QMessageBox::warning(this, "Lindy", "Could not save " + QDir::toNativeSeparators(m_ini.path()));
}
