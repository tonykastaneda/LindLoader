#include "ControlLayouts.h"

#include "Bindings.h"
#include "IniFile.h"

namespace ControlLayouts
{
namespace
{
Binding b(const QString &key, const QString &section, bool analog = false)
{
    return Binding{key, section, analog};
}

// A row with a binding per player: "P%1_Up" becomes P1_Up and P2_Up.
Row both(const QString &label, const QString &key, const QString &section, bool analog = false)
{
    return Row{label, b(key.arg(1), section, analog), b(key.arg(2), section, analog)};
}

Row p1(const QString &label, const QString &key, const QString &section, bool analog = false)
{
    return Row{label, b(key, section, analog), Binding{}};
}

Layout standard()
{
    const QString s = "Digital";
    Layout l{"Standard", s, true, {}, {}};
    l.groups << Group{"Stick", {both("Up", "P%1_Up", s), both("Down", "P%1_Down", s), both("Left", "P%1_Left", s),
                                both("Right", "P%1_Right", s)}};
    Group buttons{"Buttons", {}};
    for (int i = 1; i <= 6; ++i)
        buttons.rows << both(QStringLiteral("Button %1").arg(i), QStringLiteral("P%1_Button") + QString::number(i), s);
    l.groups << buttons;
    l.settings << Setting{"Card insert button toggles the card in and out", "Card1Insert_Toggle,Card2Insert_Toggle",
                          Setting::Check, "1"};
    return l;
}

Layout racing()
{
    const QString s = "Driving";
    Layout l{"Racing", s, false, {}, {}};
    l.groups << Group{"Steering && Pedals",
                      {p1("Steering", "P1_Steer", s, true), p1("Steer Left", "P1_Steer_Left", s),
                       p1("Steer Right", "P1_Steer_Right", s), p1("Gas", "P1_Gas", s, true),
                       p1("Brake", "P1_Brake", s, true), p1("Gas (button)", "P1_Gas_Digital", s),
                       p1("Brake (button)", "P1_Brake_Digital", s)}};
    l.groups << Group{"Buttons",
                      {p1("Gear Up", "GearUp", s), p1("Gear Down", "GearDown", s), p1("View Change", "ViewChange", s),
                       p1("Boost", "Boost", s), p1("Boost Right", "BoostRight", s),
                       p1("Music Change", "MusicChange", s)}};
    l.groups << Group{"Menu", {p1("Up", "Up", s), p1("Down", "Down", s), p1("Left", "Left", s), p1("Right", "Right", s)}};
    l.settings << Setting{"Steering deadzone", "Steer_DeadZone", Setting::Percent, "500"}
               << Setting{"Gas deadzone", "Gas_DeadZone", Setting::Percent, "500"}
               << Setting{"Brake deadzone", "Brake_DeadZone", Setting::Percent, "500"}
               << Setting{"Card insert button toggles the card in and out", "CardInsert_Toggle", Setting::Check, "1"};
    return l;
}

Layout lightgun()
{
    const QString s = "Shooting";
    Layout l{"Lightgun", s, true, {}, {}};
    l.groups << Group{"Aim", {both("Aim X", "P%1_GunX", s, true), both("Aim Y", "P%1_GunY", s, true)}};
    l.groups << Group{"Buttons",
                      {both("Trigger", "P%1_Trigger", s), both("Reload", "P%1_Reload", s),
                       both("Gun Button", "P%1_GunButton", s), both("Action Button", "P%1_ActionButton", s),
                       both("Pedal Left", "P%1_PedalLeft", s), both("Pedal Right", "P%1_PedalRight", s)}};
    l.settings << Setting{"Player 1: re-center the aim (for a stick)", "P1_GunX_Centering,P1_GunY_Centering",
                          Setting::Check, "0"}
               << Setting{"Player 2: re-center the aim (for a stick)", "P2_GunX_Centering,P2_GunY_Centering",
                          Setting::Check, "0"};
    return l;
}

Layout flight()
{
    const QString s = "Flying";
    Layout l{"Flight", s, false, {}, {}};
    l.groups << Group{"Flight Stick",
                      {p1("Stick X", "Flying_X", s, true), p1("Stick Y", "Flying_Y", s, true), p1("Left", "Flying_Left", s),
                       p1("Right", "Flying_Right", s), p1("Up", "Flying_Up", s), p1("Down", "Flying_Down", s)}};
    l.groups << Group{"Throttle",
                      {p1("Throttle", "Throttle", s, true), p1("Accelerate", "Throttle_Accelerate", s),
                       p1("Slow Down", "Throttle_Slowdown", s)}};
    l.groups << Group{"Weapons",
                      {p1("Gun", "GunTrigger", s), p1("Missile", "MissileTrigger", s),
                       p1("Climax Switch", "ClimaxSwitch", s)}};
    l.settings << Setting{"Stick X deadzone", "FLYING_X_DeadZone", Setting::Percent, "500"}
               << Setting{"Stick Y deadzone", "FLYING_Y_DeadZone", Setting::Percent, "500"}
               << Setting{"Throttle deadzone", "Throttle_DeadZone", Setting::Percent, "500"};
    return l;
}

struct Mapping
{
    QString key, section;
    QStringList controls; // without the "GC<n>_" prefix; empty clears the controller bindings
};

// Automap: the standard layout on an Xbox-style pad for player (1 or 2).
QList<Mapping> automapFor(const Layout &layout, int player)
{
    const QString p = QString::number(player);
    const QString d = "Digital", c = "Common";
    QList<Mapping> m = {{"P" + p + "_Start", c, {"BUTTON_START"}}, {"P" + p + "_Coin", c, {"BUTTON_BACK"}},
                        {"P" + p + "_Service", c, {}}};
    if (layout.name == "Standard")
    {
        m << Mapping{"P" + p + "_Up", d, {"BUTTON_DPUP", "AXIS_LEFTY_NEGATIVE"}}
          << Mapping{"P" + p + "_Down", d, {"BUTTON_DPDOWN", "AXIS_LEFTY_POSITIVE"}}
          << Mapping{"P" + p + "_Left", d, {"BUTTON_DPLEFT", "AXIS_LEFTX_NEGATIVE"}}
          << Mapping{"P" + p + "_Right", d, {"BUTTON_DPRIGHT", "AXIS_LEFTX_POSITIVE"}};
        const char *buttons[] = {"BUTTON_A", "BUTTON_B", "BUTTON_X", "BUTTON_Y", "BUTTON_RIGHTSHOULDER",
                                 "BUTTON_LEFTSHOULDER"};
        for (int i = 0; i < 6; ++i)
            m << Mapping{"P" + p + "_Button" + QString::number(i + 1), d, {buttons[i]}};
        m << Mapping{player == 1 ? "P1_Card1Insert" : "P2_Card2Insert", d, {"BUTTON_GUIDE"}};
    }
    else if (layout.name == "Racing")
    {
        const QString r = "Driving";
        m << Mapping{"P1_Steer", r, {"AXIS_LEFTX"}} << Mapping{"P1_Gas", r, {"AXIS_RIGHTTRIGGER"}}
          << Mapping{"P1_Brake", r, {"AXIS_LEFTTRIGGER"}} << Mapping{"P1_Steer_Left", r, {}}
          << Mapping{"P1_Steer_Right", r, {}} << Mapping{"P1_Gas_Digital", r, {}}
          << Mapping{"P1_Brake_Digital", r, {}} << Mapping{"GearUp", r, {"BUTTON_RIGHTSHOULDER"}}
          << Mapping{"GearDown", r, {"BUTTON_LEFTSHOULDER"}} << Mapping{"ViewChange", r, {"BUTTON_Y"}}
          << Mapping{"Boost", r, {"BUTTON_A"}} << Mapping{"BoostRight", r, {"BUTTON_B"}}
          << Mapping{"MusicChange", r, {"BUTTON_X"}} << Mapping{"Up", r, {"BUTTON_DPUP"}}
          << Mapping{"Down", r, {"BUTTON_DPDOWN"}} << Mapping{"Left", r, {"BUTTON_DPLEFT"}}
          << Mapping{"Right", r, {"BUTTON_DPRIGHT"}} << Mapping{"CardInsert", r, {"BUTTON_GUIDE"}};
    }
    else if (layout.name == "Lightgun")
    {
        const QString g = "Shooting";
        m << Mapping{"P" + p + "_GunX", g, {"AXIS_LEFTX"}} << Mapping{"P" + p + "_GunY", g, {"AXIS_LEFTY"}}
          << Mapping{"P" + p + "_Trigger", g, {"AXIS_RIGHTTRIGGER_POSITIVE"}}
          << Mapping{"P" + p + "_Reload", g, {"AXIS_LEFTTRIGGER_POSITIVE"}}
          << Mapping{"P" + p + "_GunButton", g, {"BUTTON_A"}} << Mapping{"P" + p + "_ActionButton", g, {"BUTTON_B"}}
          << Mapping{"P" + p + "_PedalLeft", g, {"BUTTON_LEFTSHOULDER"}}
          << Mapping{"P" + p + "_PedalRight", g, {"BUTTON_RIGHTSHOULDER"}};
    }
    else if (layout.name == "Flight")
    {
        const QString f = "Flying";
        m << Mapping{"Flying_X", f, {"AXIS_LEFTX"}} << Mapping{"Flying_Y", f, {"AXIS_LEFTY"}}
          << Mapping{"Throttle", f, {"AXIS_RIGHTTRIGGER_POSITIVE_HALF", "AXIS_LEFTTRIGGER_NEGATIVE_HALF"}}
          << Mapping{"GunTrigger", f, {"BUTTON_A"}} << Mapping{"MissileTrigger", f, {"BUTTON_B"}}
          << Mapping{"ClimaxSwitch", f, {"BUTTON_Y"}};
    }
    return m;
}
} // namespace

const Layout *forType(const QString &type)
{
    static const Layout layouts[] = {standard(), racing(), lightgun(), flight()};
    static const char *const types[] = {"digital", "driving", "shooting", "flying"};
    for (int i = 0; i < 4; ++i)
        if (type == QLatin1String(types[i]))
            return &layouts[i];
    return nullptr;
}

Group systemButtons(const Layout &layout)
{
    const QString c = "Common";
    Group g{"System Buttons", {}};
    if (layout.twoPlayers)
        g.rows << both("Coin", "P%1_Coin", c) << both("Start", "P%1_Start", c) << both("Service", "P%1_Service", c);
    else
        g.rows << p1("Coin", "P1_Coin", c) << p1("Start", "P1_Start", c) << p1("Service", "P1_Service", c);
    if (layout.name == "Standard")
        g.rows << Row{"Card", b("P1_Card1Insert", "Digital"), b("P2_Card2Insert", "Digital")};
    else if (layout.name == "Racing")
        g.rows << p1("Card", "CardInsert", "Driving");
    // Test and Exit Game are hotkeys: Controller Settings > Hotkeys, the same for every game.
    return g;
}

void automap(IniFile &ini, const Layout &layout, int player)
{
    const QString prefix = QStringLiteral("GC%1_").arg(player - 1);
    for (const Mapping &m : automapFor(layout, player))
    {
        QStringList controls;
        for (const QString &c : m.controls)
            controls << prefix + c;
        ini.setValue(m.section, m.key, Bindings::replaceController(ini.value(m.section, m.key, QString()), controls, player - 1));
    }
    // A stick doesn't stay where it was pointed like a mouse does, so re-center the aim.
    if (layout.name == "Lightgun")
    {
        ini.setValue("Config", QStringLiteral("P%1_GunX_Centering").arg(player), "1");
        ini.setValue("Config", QStringLiteral("P%1_GunY_Centering").arg(player), "1");
    }
}

void applyStarterBindings(IniFile &ini, const QString &type)
{
    const Layout *layout = forType(type);
    if (!layout)
        return;
    // Player 1 of a lightgun game aims with the mouse; a pad's stick would fight it.
    if (layout->name != "Lightgun")
        automap(ini, *layout, 1);
    if (layout->twoPlayers)
        automap(ini, *layout, 2);
    if (layout->name == "Standard")
    {
        // A keyboard block for player 2, beside player 1's arrows and Q/W/E.
        const QList<QPair<QString, QString>> keys = {{"P2_Up", "KEY_I"},      {"P2_Down", "KEY_K"},
                                                     {"P2_Left", "KEY_J"},    {"P2_Right", "KEY_L"},
                                                     {"P2_Button1", "KEY_U"}, {"P2_Button2", "KEY_O"},
                                                     {"P2_Button3", "KEY_P"}};
        for (const auto &k : keys)
            ini.setValue("Digital", k.first, Bindings::replace(ini.value("Digital", k.first, QString()), k.second, 1));
    }
}
} // namespace ControlLayouts
