#include "txd_reader.h"

#include <QByteArray>
#include <QFile>

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

constexpr quint32 kChunkStruct = 0x00000001u;
constexpr quint32 kChunkTextureNative = 0x00000015u;
constexpr quint32 kChunkTexDictionary = 0x00000016u;

constexpr int kChunkHeaderSize = 12;

quint32 readLe32(const QByteArray& data, int offset) {
    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData() + offset);
    return static_cast<quint32>(raw[0]) |
           (static_cast<quint32>(raw[1]) << 8) |
           (static_cast<quint32>(raw[2]) << 16) |
           (static_cast<quint32>(raw[3]) << 24);
}

quint16 readLe16(const QByteArray& data, int offset) {
    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData() + offset);
    return static_cast<quint16>(raw[0]) | (static_cast<quint16>(raw[1]) << 8);
}

QString readFixedString(const QByteArray& data, int offset, int size) {
    const QByteArray slice = data.mid(offset, size);
    const int end = slice.indexOf('\0');
    const QByteArray trimmed = end >= 0 ? slice.left(end) : slice;
    return QString::fromLatin1(trimmed).trimmed();
}

QRgb rgb565ToRgb888(quint16 value) {
    const int r = ((value >> 11) & 0x1F) * 255 / 31;
    const int g = ((value >> 5) & 0x3F) * 255 / 63;
    const int b = (value & 0x1F) * 255 / 31;
    return qRgba(r, g, b, 255);
}

QImage decodeDXT1(const QByteArray& compressed, int width, int height) {
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const auto* bytes = reinterpret_cast<const unsigned char*>(compressed.constData());
    const int blocksWide = (width + 3) / 4;
    const int blocksHigh = (height + 3) / 4;

    int offset = 0;
    for (int by = 0; by < blocksHigh; ++by) {
        for (int bx = 0; bx < blocksWide; ++bx) {
            const quint16 c0 = static_cast<quint16>(bytes[offset]) | (static_cast<quint16>(bytes[offset + 1]) << 8);
            const quint16 c1 = static_cast<quint16>(bytes[offset + 2]) | (static_cast<quint16>(bytes[offset + 3]) << 8);
            const quint32 indices = static_cast<quint32>(bytes[offset + 4]) |
                                    (static_cast<quint32>(bytes[offset + 5]) << 8) |
                                    (static_cast<quint32>(bytes[offset + 6]) << 16) |
                                    (static_cast<quint32>(bytes[offset + 7]) << 24);
            offset += 8;

            std::array<QRgb, 4> palette{};
            palette[0] = rgb565ToRgb888(c0);
            palette[1] = rgb565ToRgb888(c1);

            const int r0 = qRed(palette[0]);
            const int g0 = qGreen(palette[0]);
            const int b0 = qBlue(palette[0]);
            const int r1 = qRed(palette[1]);
            const int g1 = qGreen(palette[1]);
            const int b1 = qBlue(palette[1]);

            if (c0 > c1) {
                palette[2] = qRgba((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
                palette[3] = qRgba((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
            } else {
                palette[2] = qRgba((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
                palette[3] = qRgba(0, 0, 0, 0);
            }

            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const int x = bx * 4 + px;
                    const int y = by * 4 + py;
                    if (x >= width || y >= height) {
                        continue;
                    }

                    const int selector = (indices >> (2 * (py * 4 + px))) & 0x3;
                    image.setPixel(x, y, palette[selector]);
                }
            }
        }
    }

    return image;
}

bool parseTextureNative(const QByteArray& payload, TxdTexture* outTexture, QString* errorMessage) {
    if (payload.size() < kChunkHeaderSize) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TextureNative payload is too small");
        }
        return false;
    }

    const quint32 innerType = readLe32(payload, 0);
    const quint32 innerSize = readLe32(payload, 4);
    if (innerType != kChunkStruct || payload.size() < kChunkHeaderSize + static_cast<int>(innerSize)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TextureNative does not contain a valid STRUCT chunk");
        }
        return false;
    }

    const QByteArray structData = payload.mid(kChunkHeaderSize, static_cast<int>(innerSize));
    if (structData.size() < 64) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TextureNative STRUCT chunk is too small");
        }
        return false;
    }

    QString name = readFixedString(structData, 8, 32);
    if (name.isEmpty()) {
        name = QStringLiteral("unnamed");
    }

    const int dxtOffset = structData.indexOf("DXT1");
    if (dxtOffset < 0 || dxtOffset + 16 > structData.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Texture %1 does not contain a DXT1 header").arg(name);
        }
        return false;
    }

    const int width = readLe16(structData, dxtOffset + 4);
    const int height = readLe16(structData, dxtOffset + 6);
    const quint32 dataSize = readLe32(structData, dxtOffset + 12);
    const int dataOffset = dxtOffset + 16;

    if (width <= 0 || height <= 0 || dataOffset + static_cast<int>(dataSize) > structData.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Texture %1 has invalid dimensions or payload size").arg(name);
        }
        return false;
    }

    outTexture->name = name;
    outTexture->image = decodeDXT1(structData.mid(dataOffset, static_cast<int>(dataSize)), width, height);
    return !outTexture->image.isNull();
}

} // namespace

bool TxdReader::loadFromBytes(const QByteArray& bytes, const QString& sourceName, QString* errorMessage) {
    if (bytes.size() < kChunkHeaderSize) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TXD file is too small: %1").arg(sourceName);
        }
        return false;
    }

    const quint32 topType = readLe32(bytes, 0);
    const quint32 topSize = readLe32(bytes, 4);
    if (topType != kChunkTexDictionary || bytes.size() < kChunkHeaderSize + static_cast<int>(topSize)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("File is not a valid RenderWare TXD: %1").arg(sourceName);
        }
        return false;
    }

    const QByteArray txdPayload = bytes.mid(kChunkHeaderSize, static_cast<int>(topSize));
    QVector<TxdTexture> parsedTextures;

    int offset = 0;
    while (offset + kChunkHeaderSize <= txdPayload.size()) {
        const quint32 type = readLe32(txdPayload, offset);
        const quint32 size = readLe32(txdPayload, offset + 4);
        const int nextOffset = offset + kChunkHeaderSize + static_cast<int>(size);
        if (nextOffset > txdPayload.size()) {
            break;
        }

        if (type == kChunkTextureNative) {
            TxdTexture texture;
            QString textureError;
            if (!parseTextureNative(txdPayload.mid(offset + kChunkHeaderSize, static_cast<int>(size)), &texture, &textureError)) {
                if (errorMessage) {
                    *errorMessage = textureError;
                }
                return false;
            }
            parsedTextures.push_back(texture);
        }

        offset = nextOffset;
    }

    if (parsedTextures.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No TextureNative chunks found in %1").arg(sourceName);
        }
        return false;
    }

    sourcePath_ = sourceName;
    textures_ = std::move(parsedTextures);
    return true;
}

bool TxdReader::load(const QString& path, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open TXD file: %1").arg(path);
        }
        return false;
    }

    return loadFromBytes(file.readAll(), path, errorMessage);
}

const QVector<TxdTexture>& TxdReader::textures() const noexcept {
    return textures_;
}

const QString& TxdReader::sourcePath() const noexcept {
    return sourcePath_;
}
