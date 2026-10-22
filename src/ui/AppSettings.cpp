#include "AppSettings.h"

namespace
{
struct Default
{
    const char *key;
    QVariant value;
};

const QList<Default> &defaults()
{
    static const QList<Default> list = {
        // Interface
        {"interface/confirmExit", true},
        {"interface/inhibitScreensaver", true},
        {"interface/hideMainWindow", false},
        {"interface/theme", "dark"},
        {"interface/background", ""},
        {"interface/backgroundOpacity", 50},
        {"interface/backgroundScaling", "fill"},
        // Game list
        {"library/excluded", QStringList()},
        // Audio
        {"audio/driver", ""},
        {"audio/device", ""},
        {"audio/bufferMs", 0},
        {"audio/volume", 100},
        {"audio/mute", false},
        // Network (the loader's defaults)
        {"network/enabled", true},
        {"network/or2Ip", "192.168.1.2"},
        {"network/or2Netmask", "255.255.255.0"},
        {"network/idSeat1", "192.168.1.2"},
        {"network/idSeat2", "192.168.1.3"},
        {"network/cab1", "192.168.1.2"},
        {"network/cab2", "192.168.1.3"},
        {"network/cab3", "192.168.1.4"},
        {"network/cab4", "192.168.1.5"},
        {"network/spicy1", "192.168.1.2"},
        {"network/spicy2", "192.168.1.3"},
        {"network/srtvIp", "192.168.1.2"},
        // Folders: empty means the default place
        {"folders/covers", ""},
        {"folders/saves", ""},
        {"folders/cards", ""},
        // Controller global settings
        {"input/sdlRawInput", false},
        {"input/enhancedMode", true},
        {"input/hidapi", true},
        {"input/xinput", true},
        {"input/directinput", true},
        {"input/rawMice", false},
        // Lightgun crosshairs: empty paths mean Lindy's own red (P1) and blue (P2) ones
        {"crosshairs/enabled", false},
        {"crosshairs/p1", ""},
        {"crosshairs/p2", ""},
        {"crosshairs/size", 64},
        // Hotkeys (controls.ini [Common])
        {"hotkeys/ToggleFullscreen", "KEY_F11, KEY_Left Alt + KEY_Return"},
        {"hotkeys/ToggleFpsOverlay", "KEY_F12"},
        {"hotkeys/Test", "KEY_T"},
        {"hotkeys/ExitGame", "KEY_Escape, GC0_BUTTON_START + GC0_BUTTON_BACK, JOY0_BUTTON_11 + JOY0_BUTTON_10"},
    };
    return list;
}
} // namespace

namespace AppSettings
{
QVariant defaultValue(const QString &key)
{
    for (const Default &d : defaults())
        if (key == QLatin1String(d.key))
            return d.value;
    return {};
}

QStringList keys(const QString &group)
{
    QStringList out;
    for (const Default &d : defaults())
        if (QString::fromLatin1(d.key).startsWith(group + "/"))
            out << QString::fromLatin1(d.key);
    return out;
}

QList<Hotkey> hotkeys()
{
    return {{"ToggleFullscreen", "Navigation", "Toggle Fullscreen"},
            {"ToggleFpsOverlay", "Navigation", "Toggle FPS Overlay"},
            {"Test", "System", "Test Menu"},
            {"ExitGame", "System", "Exit Game"}};
}
} // namespace AppSettings
