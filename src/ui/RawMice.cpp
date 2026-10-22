#include "RawMice.h"

#include <QLabel>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <windows.h>
#include <hidsdi.h>

#include <vector>

namespace
{
QString devicePath(HANDLE device)
{
    UINT length = 0;
    GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &length);
    if (length == 0)
        return {};
    std::vector<wchar_t> buffer(length + 1);
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, buffer.data(), &length) == UINT(-1))
        return {};
    return QString::fromWCharArray(buffer.data());
}

QString productName(const QString &path)
{
    HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        wchar_t name[128] = {};
        const bool ok = HidD_GetProductString(h, name, sizeof(name));
        CloseHandle(h);
        if (ok && name[0])
            return QString::fromWCharArray(name).trimmed();
    }
    // No HID product string: name it by its USB IDs.
    static const QRegularExpression ids("VID_([0-9A-F]{4}).*PID_([0-9A-F]{4})", QRegularExpression::CaseInsensitiveOption);
    const auto m = ids.match(path);
    return m.hasMatch() ? QStringLiteral("Mouse (%1:%2)").arg(m.captured(1), m.captured(2)) : QStringLiteral("Mouse");
}
} // namespace

namespace RawMice
{
QList<Mouse> list()
{
    QList<Mouse> out;
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0)
        return out;
    std::vector<RAWINPUTDEVICELIST> devices(count);
    count = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (count == UINT(-1))
        return out;
    for (UINT i = 0; i < count; ++i)
    {
        if (devices[i].dwType != RIM_TYPEMOUSE)
            continue;
        const QString path = devicePath(devices[i].hDevice);
        // Remote Desktop's virtual mouse isn't a mouse anyone holds.
        if (path.isEmpty() || path.contains("RDP_MOU", Qt::CaseInsensitive))
            continue;
        out << Mouse{path, productName(path)};
    }
    return out;
}

QString nameFor(const QString &path)
{
    for (const Mouse &m : list())
        if (m.path.compare(path, Qt::CaseInsensitive) == 0)
            return m.name;
    return {};
}
} // namespace RawMice

MouseIdentifyDialog::MouseIdentifyDialog(const QString &prompt, QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Identify Mouse");
    // Any click identifies the mouse, so cancelling is a key: Esc.
    auto *text = new QLabel(prompt + "<br><br><small>Esc cancels.</small>");
    text->setAlignment(Qt::AlignCenter);
    text->setMinimumSize(380, 110);
    auto *l = new QVBoxLayout(this);
    l->addWidget(text);
}

void MouseIdentifyDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    listen(true);
}

void MouseIdentifyDialog::done(int result)
{
    listen(false);
    QDialog::done(result);
}

void MouseIdentifyDialog::listen(bool on)
{
    if (on == m_listening)
        return;
    // Raw input from every mouse, wherever it clicks.
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01; // generic desktop
    rid.usUsage = 0x02;     // mouse
    rid.dwFlags = on ? RIDEV_INPUTSINK : RIDEV_REMOVE;
    rid.hwndTarget = on ? reinterpret_cast<HWND>(winId()) : nullptr;
    m_listening = RegisterRawInputDevices(&rid, 1, sizeof(rid)) ? on : m_listening;
}

bool MouseIdentifyDialog::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
    const MSG *msg = static_cast<const MSG *>(message);
    if (type == "windows_generic_MSG" && msg->message == WM_INPUT)
    {
        RAWINPUT input = {};
        UINT size = sizeof(input);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) !=
                UINT(-1) &&
            input.header.dwType == RIM_TYPEMOUSE &&
            (input.data.mouse.usButtonFlags &
             (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_DOWN)))
        {
            m_path = devicePath(input.header.hDevice);
            if (!m_path.isEmpty())
                QMetaObject::invokeMethod(this, &QDialog::accept, Qt::QueuedConnection);
        }
    }
    return QDialog::nativeEvent(type, message, result);
}
