#include "SettingsWindow.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "GameLibrary.h"
#include "Theme.h"

SettingsWindow::SettingsWindow(GameLibrary &library, const QString &title, QWidget *parent)
    : QDialog(parent), m_library(library)
{
    setWindowTitle(title);
    resize(1000, 760);

    m_list = new QListWidget;
    m_list->setFixedWidth(220);
    m_list->setIconSize(QSize(32, 32));
    m_list->setSpacing(2);
    m_list->setStyleSheet("QListWidget { font-size: 11pt; }");
    m_pages = new QStackedWidget;
    // Follow the highlighted page; selecting one through accessibility tools doesn't move the current row.
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        if (m_list->selectedItems().isEmpty())
            return;
        const int page = m_list->row(m_list->selectedItems().first());
        m_pages->setCurrentIndex(page);
        showHelp(m_pageHelp[page].first, m_pageHelp[page].second);
    });

    auto *helpBox = new QFrame;
    helpBox->setFrameShape(QFrame::StyledPanel);
    helpBox->setMinimumHeight(150);
    m_helpTitle = new QLabel;
    m_helpTitle->setStyleSheet("font-weight: bold;");
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    m_helpText = new QLabel;
    m_helpText->setWordWrap(true);
    m_helpText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto *hl = new QVBoxLayout(helpBox);
    hl->addWidget(m_helpTitle);
    hl->addWidget(line);
    hl->addWidget(m_helpText, 1);

    auto *restore = new QPushButton(Theme::icon("reset"), " Restore Defaults");
    connect(restore, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Restore Defaults",
                                  "Restore all of the settings in this window to their defaults?") != QMessageBox::Yes)
            return;
        for (const QString &group : m_groups)
            for (const QString &key : AppSettings::keys(group))
                m_library.removeSetting(key);
        reload();
        emit settingsChanged();
    });
    auto *close = new QPushButton("Close");
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    m_footer = new QHBoxLayout;
    m_footer->addWidget(restore);
    m_footer->addStretch();
    m_footer->addWidget(close);

    auto *body = new QHBoxLayout;
    body->addWidget(m_list);
    body->addWidget(m_pages, 1);
    auto *root = new QVBoxLayout(this);
    root->addLayout(body, 1);
    root->addWidget(helpBox);
    root->addLayout(m_footer);
}

void SettingsWindow::showPage(int page)
{
    m_list->setCurrentRow(page);
}

void SettingsWindow::addPage(const QString &icon, const QString &title, const QString &description, QWidget *page)
{
    m_list->addItem(new QListWidgetItem(Theme::icon(icon), title));
    // Pages scroll when the window is short.
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    m_pages->addWidget(scroll);
    m_pageHelp << qMakePair(title.endsWith("Settings") ? title : title + " Settings", description);
    if (m_list->count() == 1)
        showPage(0);
}

void SettingsWindow::help(QWidget *widget, const QString &title, const QString &text)
{
    widget->setProperty("helpTitle", title);
    widget->setProperty("helpText", text);
    widget->installEventFilter(this);
}

bool SettingsWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Enter && watched->property("helpTitle").isValid())
        showHelp(watched->property("helpTitle").toString(), watched->property("helpText").toString());
    else if (event->type() == QEvent::Leave && watched->property("helpTitle").isValid() && m_pages->currentIndex() >= 0)
        showHelp(m_pageHelp[m_pages->currentIndex()].first, m_pageHelp[m_pages->currentIndex()].second);
    return QDialog::eventFilter(watched, event);
}

void SettingsWindow::showHelp(const QString &title, const QString &text)
{
    m_helpTitle->setText(title);
    m_helpText->setText(text);
}

QCheckBox *SettingsWindow::check(const QString &text, const QString &key, const QString &helpText)
{
    auto *box = new QCheckBox(text);
    onLoad([this, box, key] { box->setChecked(m_library.setting(key).toBool()); });
    connect(box, &QCheckBox::toggled, this, [this, key](bool on) { changed(key, on); });
    help(box, text, helpText);
    return box;
}

void SettingsWindow::onLoad(const std::function<void()> &load)
{
    m_loaders << load;
    m_loading = true;
    load();
    m_loading = false;
}

void SettingsWindow::changed(const QString &key, const QVariant &value)
{
    if (m_loading)
        return;
    m_library.setSetting(key, value);
    emit settingsChanged();
}

void SettingsWindow::reload()
{
    m_loading = true;
    for (const auto &load : m_loaders)
        load();
    m_loading = false;
}
