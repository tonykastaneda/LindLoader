#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>

// Lindy's own settings (Lindy.ini), as opposed to a game's linuxloader.ini and controls.ini. The
// Settings and Controller Settings windows edit them; GameLibrary::applySettings() hands the ones
// the loader needs to a game when it starts.
namespace AppSettings
{
// The value a key has until it's changed, e.g. defaultValue("audio/volume") == 100.
QVariant defaultValue(const QString &key);

// The keys of one group ("interface", "audio", ...), for Restore Defaults.
QStringList keys(const QString &group);

// The hotkeys of the Hotkeys page: controls.ini [Common] key and title, in page order.
struct Hotkey
{
    const char *key;
    const char *section; // heading on the Hotkeys page
    const char *title;
};
QList<Hotkey> hotkeys();
} // namespace AppSettings
