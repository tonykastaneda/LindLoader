#pragma once

#include <QList>
#include <QString>

class IniFile;

// The control layouts of the Controls page, by the game type linuxloader --identify reports:
// Standard (stick and buttons), Racing, Lightgun and Flight. Each names the controls.ini keys its
// rows edit, the controller layout Automap fills in, and the settings it shows.
namespace ControlLayouts
{
struct Binding
{
    QString key;     // controls.ini key, e.g. "P2_Button1"; empty: no binding in this column
    QString section; // controls.ini section
    bool analog = false;
};

struct Row
{
    QString label;
    Binding p1, p2;
};

struct Group
{
    QString title;
    QList<Row> rows;
};

struct Setting
{
    enum Kind
    {
        Percent, // a 0-32767 [Config] value shown as 0-100 %
        Check,   // a 0/1 [Config] value
    };
    QString label;
    QString key; // in [Config]
    Kind kind;
    QString fallback;
};

struct Layout
{
    QString name;       // "Standard", "Racing", ...
    QString section;    // the type's controls.ini section
    bool twoPlayers = false;
    QList<Group> groups; // after System Buttons
    QList<Setting> settings;
};

// The layout for a game type, or null for types without a page (mahjong/quiz).
const Layout *forType(const QString &type);

// The System Buttons group (coin, start, service, card, test, exit) for a layout.
Group systemButtons(const Layout &layout);

// Fills in player (1 or 2)'s controller bindings with the standard layout on that player's
// controller (GC0 for player 1, GC1 for player 2). Keyboard and mouse bindings stay.
void automap(IniFile &ini, const Layout &layout, int player);

// The bindings a game starts with: Linux Loader's defaults plus Automap for both players and,
// for Standard games, a keyboard block for player 2.
void applyStarterBindings(IniFile &ini, const QString &type);
} // namespace ControlLayouts
