#include "img_archive.h"

#include <QFile>

namespace {

constexpr quint32 kSectorSize = 2048;
constexpr int kHeaderSize = 8;
constexpr int kEntrySize = 32;

quint32 readLe32(const QByteArray& data, int offset) {
    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData() + offset);
    return static_cast<quint32>(raw[0]) |
           (static_cast<quint32>(raw[1]) << 8) |
           (static_cast<quint32>(raw[2]) << 16) |
           (static_cast<quint32>(raw[3]) << 24);
}

QString readFixedString(const QByteArray& data, int offset, int size) {
    const QByteArray slice = data.mid(offset, size);
    const int end = slice.indexOf('\0');
    const QByteArray trimmed = end >= 0 ? slice.left(end) : slice;
    return QString::fromLatin1(trimmed);
}

} // namespace

bool ImgArchive::open(const QString& path, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open IMG archive: %1").arg(path);
        }
        return false;
    }

    const QByteArray header = file.read(kHeaderSize);
    if (header.size() != kHeaderSize || header.left(4) != "VER2") {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported IMG archive format: %1").arg(path);
        }
        return false;
    }

    const quint32 entryCount = readLe32(header, 4);
    const qint64 directoryBytes = static_cast<qint64>(entryCount) * kEntrySize;
    const QByteArray directory = file.read(directoryBytes);
    if (directory.size() != directoryBytes) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not read IMG directory from %1").arg(path);
        }
        return false;
    }

    QHash<QString, ImgArchiveEntry> parsedEntries;
    parsedEntries.reserve(static_cast<int>(entryCount));

    for (quint32 index = 0; index < entryCount; ++index) {
        const int offset = static_cast<int>(index) * kEntrySize;
        ImgArchiveEntry entry;
        entry.sectorOffset = readLe32(directory, offset);
        entry.sectorCount = readLe32(directory, offset + 4);
        entry.name = readFixedString(directory, offset + 8, 24);
        if (entry.name.isEmpty()) {
            continue;
        }
        parsedEntries.insert(normalizeName(entry.name), entry);
    }

    sourcePath_ = path;
    entries_ = std::move(parsedEntries);
    return true;
}

bool ImgArchive::hasEntry(const QString& name) const {
    return entries_.contains(normalizeName(name));
}

QByteArray ImgArchive::readEntry(const QString& name, QString* errorMessage) const {
    const QString key = normalizeName(name);
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("IMG entry not found: %1").arg(name);
        }
        return {};
    }

    QFile file(sourcePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not reopen IMG archive: %1").arg(sourcePath_);
        }
        return {};
    }

    const qint64 byteOffset = static_cast<qint64>(it->sectorOffset) * kSectorSize;
    const qint64 byteCount = static_cast<qint64>(it->sectorCount) * kSectorSize;
    if (!file.seek(byteOffset)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not seek to IMG entry: %1").arg(name);
        }
        return {};
    }

    const QByteArray payload = file.read(byteCount);
    if (payload.size() != byteCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not read IMG entry payload: %1").arg(name);
        }
        return {};
    }

    return payload;
}

const QString& ImgArchive::sourcePath() const noexcept {
    return sourcePath_;
}

QString ImgArchive::normalizeName(const QString& name) {
    return name.trimmed().toLower();
}
