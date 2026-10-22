#include "Gamepads.h"

#include <QCoreApplication>
#include <QTimer>

#include <SDL3/SDL.h>

#include <cstdlib>

namespace
{
// How far a stick, trigger or pedal has to move before it counts as a binding.
const int kAxisThreshold = 16000;

QString upper(const char *s)
{
    return QString::fromUtf8(s ? s : "").toUpper();
}
} // namespace

Gamepads &Gamepads::instance()
{
    // Owned by the application so it goes away before Qt does.
    static Gamepads *gamepads = new Gamepads(qApp);
    return *gamepads;
}

Gamepads::Gamepads(QObject *parent) : QObject(parent)
{
    // Lindy's window has the focus, not an SDL window, so SDL has to deliver background events.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    m_ok = SDL_Init(SDL_INIT_GAMEPAD);
    if (!m_ok)
        return;
    m_timer = new QTimer(this);
    m_timer->setInterval(10);
    connect(m_timer, &QTimer::timeout, this, &Gamepads::poll);
    m_timer->start();
    poll();
}

void Gamepads::addDevice(unsigned id)
{
    if (m_devices.contains(id))
        return;
    Device d;
    d.id = id;
    d.gamepad = SDL_IsGamepad(id);
    if (d.gamepad)
        m_open[id] = SDL_OpenGamepad(id);
    else
        m_open[id] = SDL_OpenJoystick(id);
    const char *name = d.gamepad ? SDL_GetGamepadNameForID(id) : SDL_GetJoystickNameForID(id);
    d.name = QString::fromUtf8(name ? name : "Controller");
    // The loader zeroes the CRC part of the GUID (characters 4-7) before it stores it.
    char guid[33];
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(id), guid, sizeof(guid));
    guid[4] = guid[5] = guid[6] = guid[7] = '0';
    d.guid = QString::fromLatin1(guid);
    m_devices.insert(id, d);
}

void Gamepads::poll()
{
    bool changed = false;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_EVENT_JOYSTICK_ADDED:
                addDevice(e.jdevice.which);
                changed = true;
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                if (void *handle = m_open.take(e.jdevice.which))
                {
                    if (m_devices.value(e.jdevice.which).gamepad)
                        SDL_CloseGamepad(static_cast<SDL_Gamepad *>(handle));
                    else
                        SDL_CloseJoystick(static_cast<SDL_Joystick *>(handle));
                }
                m_devices.remove(e.jdevice.which);
                changed = true;
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (m_capturing && !m_analog)
                    finishCapture(e.gbutton.which,
                                  "GC#_BUTTON_" + upper(SDL_GetGamepadStringForButton((SDL_GamepadButton)e.gbutton.button)));
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            {
                if (!m_capturing)
                    break;
                const int delta = e.gaxis.value - restValue(e.gaxis.which, e.gaxis.axis, true);
                if (std::abs(delta) < kAxisThreshold)
                    break;
                QString binding = "GC#_AXIS_" + upper(SDL_GetGamepadStringForAxis((SDL_GamepadAxis)e.gaxis.axis));
                if (!m_analog)
                    binding += delta > 0 ? "_POSITIVE" : "_NEGATIVE";
                finishCapture(e.gaxis.which, binding);
                break;
            }
            // Raw joysticks (wheels, pedals, arcade sticks SDL has no gamepad mapping for).
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
                if (m_capturing && !m_analog && !m_devices.value(e.jbutton.which).gamepad)
                    finishCapture(e.jbutton.which, QStringLiteral("JOY#_BUTTON_%1").arg(e.jbutton.button));
                break;
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
            {
                if (!m_capturing || m_analog || m_devices.value(e.jhat.which).gamepad)
                    break;
                const char *dir = (e.jhat.value & SDL_HAT_UP)      ? "UP"
                                  : (e.jhat.value & SDL_HAT_DOWN)  ? "DOWN"
                                  : (e.jhat.value & SDL_HAT_LEFT)  ? "LEFT"
                                  : (e.jhat.value & SDL_HAT_RIGHT) ? "RIGHT"
                                                                   : nullptr;
                if (dir)
                    finishCapture(e.jhat.which, QStringLiteral("JOY#_HAT%1_%2").arg(e.jhat.hat).arg(dir));
                break;
            }
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            {
                if (!m_capturing || m_devices.value(e.jaxis.which).gamepad)
                    break;
                const int delta = e.jaxis.value - restValue(e.jaxis.which, e.jaxis.axis, false);
                if (std::abs(delta) < kAxisThreshold)
                    break;
                QString binding = QStringLiteral("JOY#_AXIS_%1").arg(e.jaxis.axis);
                if (!m_analog)
                    binding += delta > 0 ? "_POSITIVE" : "_NEGATIVE";
                finishCapture(e.jaxis.which, binding);
                break;
            }
            default:
                break;
        }
    }
    if (changed)
        emit devicesChanged();
}

int Gamepads::restValue(unsigned id, int axis, bool gamepadAxis) const
{
    return m_rest.value((quint64(id) << 32) | (gamepadAxis ? 0x10000 : 0) | axis, 0);
}

void Gamepads::startCapture(bool analog)
{
    if (!m_ok)
        return;
    poll(); // drop events from before the capture
    m_rest.clear();
    for (const Device &d : m_devices)
    {
        void *handle = m_open.value(d.id);
        if (!handle)
            continue;
        if (d.gamepad)
        {
            for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
                m_rest[(quint64(d.id) << 32) | 0x10000 | a] =
                    SDL_GetGamepadAxis(static_cast<SDL_Gamepad *>(handle), (SDL_GamepadAxis)a);
        }
        else
        {
            auto *joy = static_cast<SDL_Joystick *>(handle);
            for (int a = 0; a < SDL_GetNumJoystickAxes(joy); ++a)
                m_rest[(quint64(d.id) << 32) | a] = SDL_GetJoystickAxis(joy, a);
        }
    }
    m_analog = analog;
    m_capturing = true;
}

void Gamepads::stopCapture()
{
    m_capturing = false;
}

void Gamepads::finishCapture(unsigned id, const QString &binding)
{
    m_capturing = false;
    emit captured(binding, m_devices.value(id).guid);
}
