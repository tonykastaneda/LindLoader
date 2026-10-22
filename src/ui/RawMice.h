#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QLabel;

// The mice Windows reports through raw input, named by their device path: what the loader's
// [MouseDevices] section stores to tell the players' mice apart (see RAW_MOUSE_INPUT).
namespace RawMice
{
struct Mouse
{
    QString path; // "\\?\HID#VID_046D&PID_C08B&MI_00#..."
    QString name; // the product name, e.g. "G502 HERO Gaming Mouse"
};

QList<Mouse> list();
QString nameFor(const QString &path); // a connected mouse's name, or an empty string
} // namespace RawMice

// "Click a button on Player 2's mouse": waits for a click from any mouse and keeps that mouse's path.
class MouseIdentifyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MouseIdentifyDialog(const QString &prompt, QWidget *parent = nullptr);
    QString path() const { return m_path; }

protected:
    void showEvent(QShowEvent *event) override;
    void done(int result) override;
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;

private:
    void listen(bool on);

    QString m_path;
    bool m_listening = false;
};
