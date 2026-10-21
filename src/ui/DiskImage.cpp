#include "DiskImage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QPair>
#include <QVector>

#include <cstring>

namespace
{
const quint16 kMagic = 0xEF53;
const quint32 kIncompatExtents = 0x40, kIncompat64Bit = 0x80;
const quint32 kInodeFlagExtents = 0x80000;
const quint16 kModeTypeMask = 0xF000, kModeDir = 0x4000, kModeFile = 0x8000, kModeLink = 0xA000;
const quint32 kRootInode = 2;

template <typename T> T get(const QByteArray &b, int offset)
{
    T v;
    std::memcpy(&v, b.constData() + offset, sizeof(T)); // ext is little-endian, like x86
    return v;
}

class Reader
{
public:
    QString error;

    bool open(const QString &path)
    {
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::ReadOnly))
            return fail("Could not open " + path);
        const QByteArray sb = read(1024, 1024);
        if (sb.size() < 1024 || get<quint16>(sb, 56) != kMagic)
            return fail("Not an ext2/ext3/ext4 image");
        m_blockSize = 1024u << get<quint32>(sb, 24);
        m_inodesPerGroup = get<quint32>(sb, 40);
        m_inodeSize = get<quint32>(sb, 76) >= 1 ? get<quint16>(sb, 88) : 128;
        const quint32 incompat = get<quint32>(sb, 96);
        m_extents = incompat & kIncompatExtents;
        const quint32 descSize = (incompat & kIncompat64Bit) ? qMax<quint16>(32, get<quint16>(sb, 254)) : 32;
        const quint64 blocks = get<quint32>(sb, 4);
        const quint32 perGroup = get<quint32>(sb, 32);
        const quint32 groups = quint32((blocks - get<quint32>(sb, 20) + perGroup - 1) / perGroup);
        const QByteArray gdt = read(quint64(get<quint32>(sb, 20) + 1) * m_blockSize, groups * descSize);
        if (gdt.size() < int(groups * descSize))
            return fail("The image is truncated");
        for (quint32 g = 0; g < groups; ++g)
        {
            quint64 table = get<quint32>(gdt, g * descSize + 8);
            if (descSize >= 64)
                table |= quint64(get<quint32>(gdt, g * descSize + 0x28)) << 32;
            m_inodeTables << table;
        }
        return true;
    }

    QByteArray inode(quint32 n)
    {
        const quint32 group = (n - 1) / m_inodesPerGroup, index = (n - 1) % m_inodesPerGroup;
        if (group >= quint32(m_inodeTables.size()))
            return QByteArray();
        return read(m_inodeTables[group] * m_blockSize + quint64(index) * m_inodeSize, 160);
    }

    static quint16 mode(const QByteArray &ino) { return get<quint16>(ino, 0); }
    static quint64 size(const QByteArray &ino) { return get<quint32>(ino, 4) | (quint64(get<quint32>(ino, 108)) << 32); }

    // The file's data as runs of (physical block, block count); block 0 is a hole.
    QList<QPair<quint64, quint64>> runs(const QByteArray &ino)
    {
        QList<QPair<quint64, quint64>> out;
        const quint64 count = (size(ino) + m_blockSize - 1) / m_blockSize;
        if (get<quint32>(ino, 32) & kInodeFlagExtents)
            extentRuns(ino.mid(40, 60), out);
        else
        {
            QVector<quint64> list;
            for (int i = 0; i < 12; ++i)
                list << get<quint32>(ino, 40 + i * 4);
            for (int level = 1; level <= 3 && quint64(list.size()) < count; ++level)
                indirect(get<quint32>(ino, 40 + (11 + level) * 4), level, count, list);
            list.resize(qMin<quint64>(count, list.size()));
            for (quint64 b : list)
            {
                if (!out.isEmpty() && b && out.last().first && out.last().first + out.last().second == b)
                    ++out.last().second;
                else if (!out.isEmpty() && !b && !out.last().first)
                    ++out.last().second;
                else
                    out << qMakePair(b, quint64(1));
            }
        }
        return out;
    }

    QByteArray read(quint64 offset, qint64 length)
    {
        if (!m_file.seek(qint64(offset)))
            return QByteArray();
        return m_file.read(length);
    }

    quint32 blockSize() const { return m_blockSize; }
    bool usesExtents() const { return m_extents; }

private:
    bool fail(const QString &e)
    {
        error = e;
        return false;
    }

    void indirect(quint64 block, int level, quint64 count, QVector<quint64> &list)
    {
        const quint32 per = m_blockSize / 4;
        if (!block)
        {
            quint64 span = 1;
            for (int i = 0; i < level; ++i)
                span *= per;
            list.resize(qMin<quint64>(count, list.size() + span), 0);
            return;
        }
        const QByteArray b = read(block * m_blockSize, m_blockSize);
        for (quint32 i = 0; i < per && quint64(list.size()) < count; ++i)
        {
            const quint32 child = b.size() >= int((i + 1) * 4) ? get<quint32>(b, i * 4) : 0;
            if (level == 1)
                list << child;
            else
                indirect(child, level - 1, count, list);
        }
    }

    void extentRuns(const QByteArray &node, QList<QPair<quint64, quint64>> &out)
    {
        if (node.size() < 12 || get<quint16>(node, 0) != 0xF30A)
            return;
        const quint16 entries = get<quint16>(node, 2), depth = get<quint16>(node, 6);
        for (int i = 0; i < entries && 12 + (i + 1) * 12 <= node.size(); ++i)
        {
            const int e = 12 + i * 12;
            if (depth == 0)
            {
                const quint32 logical = get<quint32>(node, e);
                quint16 len = get<quint16>(node, e + 4);
                const bool unwritten = len > 32768;
                if (unwritten)
                    len -= 32768;
                const quint64 start = get<quint32>(node, e + 8) | (quint64(get<quint16>(node, e + 6)) << 32);
                quint64 have = 0;
                for (const auto &r : out)
                    have += r.second;
                if (logical > have)
                    out << qMakePair(quint64(0), quint64(logical - have));
                out << qMakePair(unwritten ? 0 : start, quint64(len));
            }
            else
            {
                const quint64 child = get<quint32>(node, e + 4) | (quint64(get<quint16>(node, e + 8)) << 32);
                extentRuns(read(child * m_blockSize, m_blockSize), out);
            }
        }
    }

    QFile m_file;
    quint32 m_blockSize = 1024;
    quint32 m_inodesPerGroup = 0;
    quint32 m_inodeSize = 128;
    bool m_extents = false;
    QVector<quint64> m_inodeTables;
};

struct Entry
{
    QString path; // relative to the image root
    quint32 inode;
    quint16 mode;
};

// Every file, folder and symlink in the image, parents before children.
bool listTree(Reader &r, quint32 dirInode, const QString &prefix, QList<Entry> &out, int depth)
{
    if (depth > 64)
        return false;
    const QByteArray ino = r.inode(dirInode);
    QByteArray data;
    for (const auto &run : r.runs(ino))
        data += run.first ? r.read(run.first * r.blockSize(), run.second * r.blockSize())
                          : QByteArray(int(run.second * r.blockSize()), '\0');
    data.truncate(int(Reader::size(ino)));
    for (int pos = 0; pos + 8 <= data.size();)
    {
        const quint32 child = get<quint32>(data, pos);
        const quint16 recLen = get<quint16>(data, pos + 4);
        const quint8 nameLen = quint8(data[pos + 6]);
        if (recLen < 8)
            break;
        const QString name = QString::fromUtf8(data.constData() + pos + 8, nameLen);
        pos += recLen;
        if (!child || name == "." || name == ".." || (prefix.isEmpty() && name == "lost+found"))
            continue;
        const quint16 mode = Reader::mode(r.inode(child));
        const QString path = prefix.isEmpty() ? name : prefix + "/" + name;
        out << Entry{path, child, mode};
        if ((mode & kModeTypeMask) == kModeDir && !listTree(r, child, path, out, depth + 1))
            return false;
    }
    return true;
}
} // namespace

namespace DiskImage
{
bool isImage(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || !f.seek(1024 + 56))
        return false;
    const QByteArray m = f.read(2);
    return m.size() == 2 && get<quint16>(m, 0) == kMagic;
}

bool extract(const QString &image, const QString &dest, const std::function<void(qint64, qint64)> &progress,
             QString *error)
{
    auto fail = [error](const QString &e) {
        if (error)
            *error = e;
        return false;
    };
    Reader r;
    if (!r.open(image))
        return fail(r.error);
    QList<Entry> entries;
    if (!listTree(r, kRootInode, QString(), entries, 0))
        return fail("The image's folders nest too deep");

    qint64 total = 0, done = 0;
    for (const Entry &e : entries)
        if ((e.mode & kModeTypeMask) == kModeFile)
            total += qint64(Reader::size(r.inode(e.inode)));

    QDir root(dest);
    root.mkpath(".");
    QList<QPair<QString, QString>> links; // path, target
    const qint64 chunk = 8 << 20;
    for (const Entry &e : entries)
    {
        const QString out = root.filePath(e.path);
        const QByteArray ino = r.inode(e.inode);
        switch (e.mode & kModeTypeMask)
        {
            case kModeDir:
                root.mkpath(e.path);
                break;
            case kModeLink:
            {
                const quint64 len = Reader::size(ino);
                QByteArray target;
                if (len < 60 && r.runs(ino).isEmpty())
                    target = ino.mid(40, int(len)); // short targets live in the inode
                else
                    for (const auto &run : r.runs(ino))
                        target += r.read(run.first * r.blockSize(), run.second * r.blockSize());
                links << qMakePair(e.path, QString::fromUtf8(target.left(int(len))));
                break;
            }
            case kModeFile:
            {
                QFile f(out);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    return fail("Could not write " + QDir::toNativeSeparators(out));
                qint64 left = qint64(Reader::size(ino));
                for (const auto &run : r.runs(ino))
                {
                    for (quint64 b = 0; b < run.second && left > 0;)
                    {
                        const qint64 n = qMin<qint64>(qMin<qint64>(left, chunk), qint64(run.second - b) * r.blockSize());
                        const QByteArray data =
                            run.first ? r.read((run.first + b) * r.blockSize(), n) : QByteArray(int(n), '\0');
                        if (data.size() != n || f.write(data) != n)
                            return fail("Could not copy " + e.path);
                        b += quint64((n + r.blockSize() - 1) / r.blockSize());
                        left -= n;
                        done += n;
                        if (progress)
                            progress(done, total);
                    }
                }
                break;
            }
            default:
                break; // devices, pipes, sockets
        }
    }

    // Windows symlinks need admin rights, so a link becomes a copy of what it points to.
    for (int pass = 0; pass < 4 && !links.isEmpty(); ++pass)
    {
        for (int i = links.size() - 1; i >= 0; --i)
        {
            const QString link = root.filePath(links[i].first);
            const QString target = links[i].second.startsWith('/')
                                       ? root.filePath(links[i].second.mid(1))
                                       : QFileInfo(link).dir().filePath(links[i].second);
            const QFileInfo t(target);
            if (!t.exists())
                continue;
            if (t.isDir())
                QDir().mkpath(link); // rare in games; the folder itself is not copied
            else
                QFile::copy(t.absoluteFilePath(), link);
            links.removeAt(i);
        }
    }
    return true;
}
} // namespace DiskImage
