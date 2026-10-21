#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QTimer;

// Connected controllers, read with SDL3 the same way linuxloader.exe reads them, and capture of the
// next controller input as a controls.ini binding.
class Gamepads : public QObject
{
    Q_OBJECT
public:
    struct Device
    {
        unsigned id = 0;
        QString name;
        QString guid; // as the loader stores it in [ControllerGUIDs]
        bool gamepad = false;
    };

    static Gamepads &instance();

    QList<Device> devices() const { return m_devices.values(); }

    // Waits for the next button press or stick/trigger movement. analog: capture whole axes
    // ("GC#_AXIS_LEFTX") instead of directions ("GC#_AXIS_LEFTX_POSITIVE"). Bindings come back with
    // '#' where the player's index goes.
    void startCapture(bool analog);
    void stopCapture();

signals:
    void devicesChanged();
    void captured(const QString &binding, const QString &guid);

private:
    explicit Gamepads(QObject *parent);
    void poll();
    void addDevice(unsigned id);
    void finishCapture(unsigned id, const QString &binding);
    int restValue(unsigned id, int axis, bool gamepadAxis) const;

    QHash<unsigned, Device> m_devices;
    QHash<unsigned, void *> m_open; // SDL_Gamepad* or SDL_Joystick*
    QTimer *m_timer = nullptr;
    bool m_ok = false;
    bool m_capturing = false;
    bool m_analog = false;
    QHash<quint64, int> m_rest; // axis positions when the capture started
};
