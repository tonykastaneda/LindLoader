#include "Bindings.h"

#include <QHash>
#include <QKeyEvent>
#include <QRegularExpression>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include <iterator>

namespace
{
// Windows (set 1) scancodes 0x00-0x58 to SDL scancodes; the loader binds keys by SDL scancode name.
const SDL_Scancode kWinScancodes[] = {
    SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0,
    SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB,
    // 0x10
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U,
    SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
    SDL_SCANCODE_RETURN, SDL_SCANCODE_LCTRL, SDL_SCANCODE_A, SDL_SCANCODE_S,
    // 0x20
    SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,
    SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_GRAVE, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_BACKSLASH,
    SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
    // 0x30
    SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH,
    SDL_SCANCODE_RSHIFT, SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE, SDL_SCANCODE_CAPSLOCK,
    SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
    // 0x40
    SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_NUMLOCKCLEAR,
    SDL_SCANCODE_SCROLLLOCK, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_MINUS,
    SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_KP_1,
    // 0x50
    SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_NONUSBACKSLASH, SDL_SCANCODE_F11, SDL_SCANCODE_F12};

// Keys sent with the 0xE0 prefix (Qt sets bit 8 of the native scancode for them).
SDL_Scancode extendedScancode(unsigned code)
{
    switch (code)
    {
        case 0x1C: return SDL_SCANCODE_KP_ENTER;
        case 0x1D: return SDL_SCANCODE_RCTRL;
        case 0x35: return SDL_SCANCODE_KP_DIVIDE;
        case 0x37: return SDL_SCANCODE_PRINTSCREEN;
        case 0x38: return SDL_SCANCODE_RALT;
        case 0x47: return SDL_SCANCODE_HOME;
        case 0x48: return SDL_SCANCODE_UP;
        case 0x49: return SDL_SCANCODE_PAGEUP;
        case 0x4B: return SDL_SCANCODE_LEFT;
        case 0x4D: return SDL_SCANCODE_RIGHT;
        case 0x4F: return SDL_SCANCODE_END;
        case 0x50: return SDL_SCANCODE_DOWN;
        case 0x51: return SDL_SCANCODE_PAGEDOWN;
        case 0x52: return SDL_SCANCODE_INSERT;
        case 0x53: return SDL_SCANCODE_DELETE;
        case 0x5B: return SDL_SCANCODE_LGUI;
        case 0x5C: return SDL_SCANCODE_RGUI;
        case 0x5D: return SDL_SCANCODE_APPLICATION;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

QString padButtonName(const QString &b)
{
    static const QHash<QString, QString> names = {
        {"BACK", "Back"},          {"GUIDE", "Guide"},           {"START", "Start"},       {"LEFTSTICK", "LS"},
        {"RIGHTSTICK", "RS"},      {"LEFTSHOULDER", "LB"},       {"RIGHTSHOULDER", "RB"},  {"DPUP", "D-Pad Up"},
        {"DPDOWN", "D-Pad Down"},  {"DPLEFT", "D-Pad Left"},     {"DPRIGHT", "D-Pad Right"}, {"MISC1", "Share"},
        {"TOUCHPAD", "Touchpad"},
    };
    return names.value(b, b.size() <= 1 ? b : b.left(1) + b.mid(1).toLower());
}

QString padAxisName(QString axis)
{
    QString extra;
    if (axis.endsWith("_INVERTED"))
    {
        axis.chop(9);
        extra = " (inverted)";
    }
    if (axis.endsWith("_POSITIVE_HALF") || axis.endsWith("_NEGATIVE_HALF"))
    {
        extra = axis.endsWith("_POSITIVE_HALF") ? " (+ half)" : " (- half)";
        axis.chop(14);
    }
    int dir = 0;
    if (axis.endsWith("_POSITIVE"))
    {
        dir = 1;
        axis.chop(9);
    }
    else if (axis.endsWith("_NEGATIVE"))
    {
        dir = -1;
        axis.chop(9);
    }
    QString name;
    if (axis == "LEFTTRIGGER")
        name = "LT";
    else if (axis == "RIGHTTRIGGER")
        name = "RT";
    else if (axis == "LEFTX" || axis == "RIGHTX")
        name = (axis == "LEFTX" ? "Left Stick" : "Right Stick") + QString(dir > 0 ? " Right" : dir < 0 ? " Left" : " X");
    else if (axis == "LEFTY" || axis == "RIGHTY")
        name = (axis == "LEFTY" ? "Left Stick" : "Right Stick") + QString(dir > 0 ? " Down" : dir < 0 ? " Up" : " Y");
    else
        name = axis + (dir > 0 ? "+" : dir < 0 ? "-" : "");
    return name + extra;
}

QString singleName(const QString &b)
{
    if (b.startsWith("KEY_"))
        return b.mid(4);
    // MOUSE_ is any mouse, MOUSEn_ player n's own mouse (raw mouse input).
    static const QRegularExpression mouse("^MOUSE(\\d?)_(.+)$");
    if (const auto m = mouse.match(b); m.hasMatch())
    {
        QString what = m.captured(2);
        if (what.endsWith("_BUTTON"))
            what = what.chopped(7).toLower();
        else if (what == "BUTTON_4")
            what = "Back";
        else if (what == "BUTTON_5")
            what = "Forward";
        else if (what.startsWith("BUTTON_"))
            what = "Button " + what.mid(7);
        else if (what.startsWith("AXIS_"))
            what = what.mid(5);
        if (!what.isEmpty())
            what[0] = what[0].toUpper();
        return (m.captured(1).isEmpty() ? "Mouse " : "Mouse " + m.captured(1) + " ") + what;
    }

    static const QRegularExpression gc("^GC\\d+_(BUTTON|AXIS)_(.+)$");
    if (const auto m = gc.match(b); m.hasMatch())
        return "Pad " + (m.captured(1) == "BUTTON" ? padButtonName(m.captured(2)) : padAxisName(m.captured(2)));

    static const QRegularExpression joy("^JOY\\d+_(?:BUTTON_(\\d+)|HAT(\\d+)_(\\w+)|AXIS_(\\d+)(.*))$");
    if (const auto m = joy.match(b); m.hasMatch())
    {
        if (!m.captured(1).isEmpty())
            return "Joy Button " + m.captured(1);
        if (!m.captured(3).isEmpty())
            return "Joy Hat " + m.captured(3).left(1) + m.captured(3).mid(1).toLower();
        QString suffix = m.captured(5);
        suffix.replace("_POSITIVE_HALF", " (+ half)").replace("_NEGATIVE_HALF", " (- half)");
        suffix.replace("_POSITIVE", "+").replace("_NEGATIVE", "-").replace("_INVERTED", " (inverted)");
        return "Joy Axis " + m.captured(4) + suffix;
    }
    return b;
}

enum class Kind
{
    Keyboard,
    Mouse,
    Controller,
};

Kind kindOf(const QString &binding)
{
    if (binding.startsWith("KEY_"))
        return Kind::Keyboard;
    if (binding.startsWith("MOUSE"))
        return Kind::Mouse;
    return Kind::Controller;
}
} // namespace

namespace Bindings
{
QString fromKeyEvent(const QKeyEvent *event)
{
    const unsigned native = event->nativeScanCode();
    const unsigned code = native & 0xFF;
    SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
    if (native & 0x100)
        sc = extendedScancode(code);
    else if (code < std::size(kWinScancodes))
        sc = kWinScancodes[code];
    if (sc == SDL_SCANCODE_UNKNOWN)
        return QString();
    return "KEY_" + QString::fromUtf8(SDL_GetScancodeName(sc));
}

QString displayName(const QString &binding)
{
    QStringList parts;
    for (const QString &p : binding.split('+'))
        parts << singleName(p.trimmed());
    return parts.join(" + ");
}

QStringList split(const QString &value)
{
    QStringList result;
    for (const QString &b : value.split(','))
        if (!b.trimmed().isEmpty() && b.trimmed() != "\"\"")
            result << b.trimmed();
    return result;
}

QString replace(const QString &value, const QString &binding, int playerIndex)
{
    const QString added = QString(binding).replace('#', QString::number(playerIndex));
    const Kind kind = kindOf(added);
    const QString gc = QStringLiteral("GC%1_").arg(playerIndex), joy = QStringLiteral("JOY%1_").arg(playerIndex);

    QStringList keyboard, mouse, controllers;
    for (const QString &b : split(value))
    {
        const Kind k = kindOf(b);
        if (k == kind && (k != Kind::Controller || b.startsWith(gc) || b.startsWith(joy)))
            continue; // replaced by the new binding
        (k == Kind::Keyboard ? keyboard : k == Kind::Mouse ? mouse : controllers) << b;
    }
    (kind == Kind::Keyboard ? keyboard : kind == Kind::Mouse ? mouse : controllers) << added;
    return (keyboard + mouse + controllers).join(", ");
}

QString replaceController(const QString &value, const QStringList &controls, int playerIndex)
{
    // Only gamepad (GC) bindings: generic joysticks (JOY), such as arcade sticks SDL has no
    // gamepad mapping for, keep theirs.
    const QString gc = QStringLiteral("GC%1_").arg(playerIndex);
    QStringList kept;
    for (const QString &b : split(value))
        if (!b.startsWith(gc))
            kept << b;
    return (kept + controls).join(", ");
}
} // namespace Bindings
