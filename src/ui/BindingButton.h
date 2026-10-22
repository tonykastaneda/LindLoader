#pragma once

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>

#include <functional>

#include "Bindings.h"
#include "ControlLayouts.h"
#include "Gamepads.h"

// One binding: shows what the action is bound to; click to bind, right-click to clear.
class BindingButton : public QPushButton
{
public:
    static constexpr int CaptureSeconds = 5;

    BindingButton(const ControlLayouts::Binding &binding, int player, QWidget *parent = nullptr)
        : QPushButton(parent), binding(binding), player(player)
    {
        setMinimumWidth(150);
        setMinimumHeight(28);
        connect(this, &QPushButton::clicked, this, &BindingButton::startCapture);
        m_countdown.setInterval(1000);
        connect(&m_countdown, &QTimer::timeout, this, [this] {
            if (--m_seconds <= 0)
                stopCapture();
            else
                showPrompt();
        });
    }
    ~BindingButton() override { stopCapture(); }

    void setBindings(const QString &value)
    {
        QStringList names;
        for (const QString &b : Bindings::split(value))
            names << Bindings::displayName(b);
        m_text = names.isEmpty() ? QStringLiteral("-") : names.join(", ");
        setToolTip(names.isEmpty() ? QStringLiteral("Not bound") : names.join("\n"));
        if (!m_capturing)
            showText();
    }

    const ControlLayouts::Binding binding;
    const int player; // 1 or 2
    std::function<void(BindingButton *, const QString &, const QString &)> bound;
    std::function<void(BindingButton *)> cleared;

protected:
    bool event(QEvent *e) override
    {
        if (m_capturing && (e->type() == QEvent::KeyPress || e->type() == QEvent::ShortcutOverride))
        {
            auto *key = static_cast<QKeyEvent *>(e);
            if (e->type() == QEvent::KeyPress && !key->isAutoRepeat() && !binding.analog)
            {
                const QString b = Bindings::fromKeyEvent(key);
                if (!b.isEmpty())
                {
                    stopCapture();
                    bound(this, b, QString());
                }
            }
            e->accept();
            return true;
        }
        return QPushButton::event(e);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::RightButton)
        {
            stopCapture();
            cleared(this);
            return;
        }
        if (m_capturing)
        {
            // The middle and side buttons bind themselves (a left click doesn't restart the countdown).
            const QString b = binding.analog ? QString() : mouseButtonName(e->button());
            if (!b.isEmpty())
            {
                stopCapture();
                bound(this, b, QString());
            }
            return;
        }
        QPushButton::mousePressEvent(e);
    }

    void resizeEvent(QResizeEvent *e) override
    {
        QPushButton::resizeEvent(e);
        if (!m_capturing)
            showText();
    }

private:
    // The loader's name for a mouse button (SDL numbers them 1 left, 2 middle, 3 right, 4 and 5 the side buttons).
    static QString mouseButtonName(Qt::MouseButton button)
    {
        switch (button)
        {
        case Qt::MiddleButton:
            return QStringLiteral("MOUSE_MIDDLE_BUTTON");
        case Qt::BackButton:
            return QStringLiteral("MOUSE_BUTTON_4");
        case Qt::ForwardButton:
            return QStringLiteral("MOUSE_BUTTON_5");
        default:
            for (int n = 3; n <= 10; ++n)
                if (button == Qt::MouseButton(Qt::ExtraButton3 << (n - 3)))
                    return QStringLiteral("MOUSE_BUTTON_%1").arg(n + 3);
            return {};
        }
    }

    void showText() { setText(fontMetrics().elidedText(m_text, Qt::ElideRight, width() - 16)); }

    void showPrompt()
    {
        setText(QStringLiteral("%1... %2").arg(binding.analog ? "Move a stick or trigger" : "Press a key or button").arg(m_seconds));
    }

    void startCapture()
    {
        if (s_active && s_active != this)
            s_active->stopCapture();
        s_active = this;
        m_capturing = true;
        m_seconds = CaptureSeconds;
        showPrompt();
        setDown(true);
        grabKeyboard();
        m_countdown.start();
        m_connection = connect(&Gamepads::instance(), &Gamepads::captured, this,
                               [this](const QString &b, const QString &guid) {
                                   stopCapture();
                                   bound(this, b, guid);
                               });
        Gamepads::instance().startCapture(binding.analog);
    }

    void stopCapture()
    {
        if (!m_capturing)
            return;
        m_capturing = false;
        if (s_active == this)
            s_active = nullptr;
        Gamepads::instance().stopCapture();
        disconnect(m_connection);
        m_countdown.stop();
        releaseKeyboard();
        setDown(false);
        showText();
    }

    static inline BindingButton *s_active = nullptr;
    QString m_text;
    bool m_capturing = false;
    int m_seconds = 0;
    QTimer m_countdown;
    QMetaObject::Connection m_connection;
};
