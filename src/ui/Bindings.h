#pragma once

#include <QString>
#include <QStringList>

class QKeyEvent;

// controls.ini binding strings (see src/loader/docs/Controls-Guide.md).
namespace Bindings
{
// "KEY_<SDL scancode name>" for a key press, or an empty string for keys the loader can't bind.
QString fromKeyEvent(const QKeyEvent *event);

// A readable name for a binding: "KEY_Space" -> "Space", "GC0_BUTTON_A" -> "Pad A".
QString displayName(const QString &binding);

// Splits a controls.ini value into its bindings.
QStringList split(const QString &value);

// Replaces the bindings of the same kind (keyboard, or the given player's controller) with binding.
QString replace(const QString &value, const QString &binding, int playerIndex);
} // namespace Bindings
