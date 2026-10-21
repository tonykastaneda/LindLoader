#pragma once

#include <QString>
#include <QStringList>

// Edits linuxloader.ini / controls.ini in place: values change, comments and layout stay.
class IniFile
{
public:
    bool load(const QString &path);
    bool save() const;

    QString value(const QString &key, const QString &fallback = QString()) const;
    void setValue(const QString &key, const QString &value);

private:
    int findKey(const QString &key) const;

    QString m_path;
    QStringList m_lines;
};
