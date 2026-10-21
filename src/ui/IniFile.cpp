#include "IniFile.h"

#include <QFile>
#include <QTextStream>

bool IniFile::load(const QString &path)
{
    m_path = path;
    m_lines.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&f);
    while (!in.atEnd())
        m_lines << in.readLine();
    return true;
}

bool IniFile::save() const
{
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    for (const QString &line : m_lines)
        out << line << '\n';
    return true;
}

int IniFile::findKey(const QString &key) const
{
    for (int i = 0; i < m_lines.size(); ++i)
    {
        const QString line = m_lines[i].trimmed();
        if (line.startsWith('#') || line.startsWith(';'))
            continue;
        const int eq = line.indexOf('=');
        if (eq > 0 && line.left(eq).trimmed().compare(key, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QString IniFile::value(const QString &key, const QString &fallback) const
{
    const int i = findKey(key);
    if (i < 0)
        return fallback;
    const QString &line = m_lines[i];
    return line.mid(line.indexOf('=') + 1).trimmed();
}

void IniFile::setValue(const QString &key, const QString &value)
{
    const int i = findKey(key);
    if (i >= 0)
        m_lines[i] = key + " = " + value;
    else
        m_lines << key + " = " + value;
}
