#include "sa_map_loader.h"

#include "img_archive.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTransform>

#include <array>
#include <limits>

namespace {

QString firstExistingPath(const QStringList& candidates) {
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

QImage composeQuadrants(const QVector<TxdTexture>& textures) {
    const TxdTexture* bit1 = nullptr;
    const TxdTexture* bit2 = nullptr;
    const TxdTexture* bit3 = nullptr;
    const TxdTexture* bit4 = nullptr;

    for (const TxdTexture& texture : textures) {
        if (texture.name == QStringLiteral("gtasamapbit1")) {
            bit1 = &texture;
        } else if (texture.name == QStringLiteral("gtasamapbit2")) {
            bit2 = &texture;
        } else if (texture.name == QStringLiteral("gtasamapbit3")) {
            bit3 = &texture;
        } else if (texture.name == QStringLiteral("gtasamapbit4")) {
            bit4 = &texture;
        }
    }

    if (!bit1 || !bit2 || !bit3 || !bit4) {
        return {};
    }

    const int tileWidth = bit1->image.width();
    const int tileHeight = bit1->image.height();

    QImage composed(tileWidth * 2, tileHeight * 2, QImage::Format_ARGB32);
    composed.fill(Qt::black);

    // The quadrant order is derived from the SA-MP map tiles in samaps.txd.
    for (int y = 0; y < tileHeight; ++y) {
        for (int x = 0; x < tileWidth; ++x) {
            composed.setPixel(x, y, bit1->image.pixel(x, y));
            composed.setPixel(tileWidth + x, y, bit2->image.pixel(x, y));
            composed.setPixel(x, tileHeight + y, bit3->image.pixel(x, y));
            composed.setPixel(tileWidth + x, tileHeight + y, bit4->image.pixel(x, y));
        }
    }

    return composed;
}

QString radarEntryName(int index) {
    return QStringLiteral("radar%1.txd").arg(index, 2, 10, QLatin1Char('0'));
}

enum class Orientation {
    Identity,
    Rotate90,
    Rotate180,
    Rotate270,
    MirrorHorizontal,
    MirrorVertical,
    Transpose,
    Transverse,
};

QImage applyOrientation(const QImage& image, Orientation orientation) {
    switch (orientation) {
    case Orientation::Identity:
        return image;
    case Orientation::Rotate90:
        return image.transformed(QTransform().rotate(90));
    case Orientation::Rotate180:
        return image.transformed(QTransform().rotate(180));
    case Orientation::Rotate270:
        return image.transformed(QTransform().rotate(270));
    case Orientation::MirrorHorizontal:
        return image.flipped(Qt::Horizontal);
    case Orientation::MirrorVertical:
        return image.flipped(Qt::Vertical);
    case Orientation::Transpose:
        return image.flipped(Qt::Horizontal).transformed(QTransform().rotate(90));
    case Orientation::Transverse:
        return image.flipped(Qt::Horizontal).transformed(QTransform().rotate(270));
    }

    return image;
}

QString orientationName(Orientation orientation) {
    switch (orientation) {
    case Orientation::Identity:
        return QStringLiteral("identity");
    case Orientation::Rotate90:
        return QStringLiteral("rotate90");
    case Orientation::Rotate180:
        return QStringLiteral("rotate180");
    case Orientation::Rotate270:
        return QStringLiteral("rotate270");
    case Orientation::MirrorHorizontal:
        return QStringLiteral("mirror-horizontal");
    case Orientation::MirrorVertical:
        return QStringLiteral("mirror-vertical");
    case Orientation::Transpose:
        return QStringLiteral("transpose");
    case Orientation::Transverse:
        return QStringLiteral("transverse");
    }

    return QStringLiteral("unknown");
}

double imageDifferenceScore(const QImage& a, const QImage& b) {
    if (a.size() != b.size() || a.isNull() || b.isNull()) {
        return std::numeric_limits<double>::infinity();
    }

    const QImage a32 = a.convertToFormat(QImage::Format_ARGB32);
    const QImage b32 = b.convertToFormat(QImage::Format_ARGB32);

    long double sum = 0.0;
    const int pixelCount = a32.width() * a32.height();

    for (int y = 0; y < a32.height(); ++y) {
        const auto* aRow = reinterpret_cast<const QRgb*>(a32.constScanLine(y));
        const auto* bRow = reinterpret_cast<const QRgb*>(b32.constScanLine(y));
        for (int x = 0; x < a32.width(); ++x) {
            const int dr = qRed(aRow[x]) - qRed(bRow[x]);
            const int dg = qGreen(aRow[x]) - qGreen(bRow[x]);
            const int db = qBlue(aRow[x]) - qBlue(bRow[x]);
            sum += static_cast<long double>(dr * dr + dg * dg + db * db);
        }
    }

    return static_cast<double>(sum / static_cast<long double>(pixelCount));
}

bool loadRadarTileTextures(const QString& imgPath, QVector<TxdTexture>* outTiles, QString* errorMessage) {
    ImgArchive archive;
    if (!archive.open(imgPath, errorMessage)) {
        return false;
    }

    QVector<TxdTexture> tiles;
    tiles.reserve(144);

    for (int index = 0; index < 144; ++index) {
        const QString entryName = radarEntryName(index);
        QString entryError;
        const QByteArray txdBytes = archive.readEntry(entryName, &entryError);
        if (txdBytes.isEmpty()) {
            if (errorMessage) {
                *errorMessage = entryError;
            }
            return false;
        }

        TxdReader reader;
        const QString syntheticSource = QStringLiteral("%1::%2").arg(imgPath, entryName);
        QString txdError;
        if (!reader.loadFromBytes(txdBytes, syntheticSource, &txdError)) {
            if (errorMessage) {
                *errorMessage = txdError;
            }
            return false;
        }

        if (reader.textures().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No texture decoded from %1").arg(entryName);
            }
            return false;
        }

        tiles.push_back(reader.textures().front());
    }

    *outTiles = std::move(tiles);
    return true;
}

QImage composeRadarMap(const QVector<TxdTexture>& tiles) {
    if (tiles.size() != 144) {
        return {};
    }

    const int tileWidth = tiles.front().image.width();
    const int tileHeight = tiles.front().image.height();
    if (tileWidth <= 0 || tileHeight <= 0) {
        return {};
    }

    QImage composed(tileWidth * 12, tileHeight * 12, QImage::Format_ARGB32);
    composed.fill(Qt::black);

    for (int index = 0; index < tiles.size(); ++index) {
        const int row = index / 12;
        const int col = index % 12;
        const QImage& tile = tiles[index].image;
        if (tile.width() != tileWidth || tile.height() != tileHeight) {
            return {};
        }

        for (int y = 0; y < tileHeight; ++y) {
            for (int x = 0; x < tileWidth; ++x) {
                composed.setPixel(col * tileWidth + x, row * tileHeight + y, tile.pixel(x, y));
            }
        }
    }

    return composed;
}

bool loadHighResRadarMap(const QString& imgPath,
                         const QString& txdReferencePath,
                         SaMapAsset* outAsset,
                         QString* errorMessage) {
    QVector<TxdTexture> tiles;
    if (!loadRadarTileTextures(imgPath, &tiles, errorMessage)) {
        return false;
    }

    QImage fullMap = composeRadarMap(tiles);
    if (fullMap.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not compose radar tile grid from %1").arg(imgPath);
        }
        return false;
    }

    QString alignmentText = QStringLiteral("row-major");
    if (!txdReferencePath.isEmpty()) {
        SaMapAsset referenceMap;
        QString referenceError;
        if (SaMapLoader::loadMapFromTxd(txdReferencePath, &referenceMap, &referenceError) && !referenceMap.image.isNull()) {
            const QImage thumbnail =
                fullMap.scaled(referenceMap.image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            const std::array<Orientation, 8> orientations = {
                Orientation::Identity,
                Orientation::Rotate90,
                Orientation::Rotate180,
                Orientation::Rotate270,
                Orientation::MirrorHorizontal,
                Orientation::MirrorVertical,
                Orientation::Transpose,
                Orientation::Transverse,
            };

            Orientation bestOrientation = Orientation::Identity;
            double bestScore = std::numeric_limits<double>::infinity();

            for (Orientation orientation : orientations) {
                const QImage candidate = applyOrientation(thumbnail, orientation);
                const double score = imageDifferenceScore(candidate, referenceMap.image);
                if (score < bestScore) {
                    bestScore = score;
                    bestOrientation = orientation;
                }
            }

            fullMap = applyOrientation(fullMap, bestOrientation);
            alignmentText =
                QStringLiteral("%1 aligned against samaps.txd (score=%2)")
                    .arg(orientationName(bestOrientation))
                    .arg(bestScore, 0, 'f', 2);
        }
    }

    outAsset->image = std::move(fullMap);
    outAsset->description =
        QStringLiteral("%1 via gta3.img radar tiles (%2x%3, %4)")
            .arg(imgPath)
            .arg(outAsset->image.width())
            .arg(outAsset->image.height())
            .arg(alignmentText);
    return true;
}

bool alignImageToReferenceMap(QImage* image, const QString& txdReferencePath, QString* alignmentText) {
    if (!image || image->isNull()) {
        return false;
    }

    if (alignmentText) {
        *alignmentText = QStringLiteral("unaligned");
    }

    if (txdReferencePath.isEmpty()) {
        return true;
    }

    SaMapAsset referenceMap;
    QString referenceError;
    if (!SaMapLoader::loadMapFromTxd(txdReferencePath, &referenceMap, &referenceError) || referenceMap.image.isNull()) {
        return false;
    }

    const QImage thumbnail = image->scaled(referenceMap.image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const std::array<Orientation, 8> orientations = {
        Orientation::Identity,
        Orientation::Rotate90,
        Orientation::Rotate180,
        Orientation::Rotate270,
        Orientation::MirrorHorizontal,
        Orientation::MirrorVertical,
        Orientation::Transpose,
        Orientation::Transverse,
    };

    Orientation bestOrientation = Orientation::Identity;
    double bestScore = std::numeric_limits<double>::infinity();

    for (Orientation orientation : orientations) {
        const QImage candidate = applyOrientation(thumbnail, orientation);
        const double score = imageDifferenceScore(candidate, referenceMap.image);
        if (score < bestScore) {
            bestScore = score;
            bestOrientation = orientation;
        }
    }

    *image = applyOrientation(*image, bestOrientation);
    if (alignmentText) {
        *alignmentText =
            QStringLiteral("%1 aligned against samaps.txd (score=%2)")
                .arg(orientationName(bestOrientation))
                .arg(bestScore, 0, 'f', 2);
    }
    return true;
}

} // namespace

QString SaMapLoader::findDefaultImgPath() {
    return firstExistingPath({
        QStringLiteral("/home/chairman/Games/grand-theft-auto-san-andreas-orig/drive_c/Program Files/Rockstar Games/Grand Theft Auto San Andreas/models/gta3.img"),
        QStringLiteral("/home/chairman/Games/grand-theft-auto-san-andreas-orig/drive_c/Program Files/Rockstar Games/Grand Theft Auto San Andreas.bak/models/gta3.img"),
    });
}

QString SaMapLoader::findDefaultTxdPath() {
    return firstExistingPath({
        QStringLiteral("/home/chairman/Games/grand-theft-auto-san-andreas-orig/drive_c/Program Files/Rockstar Games/Grand Theft Auto San Andreas/SAMP/samaps.txd"),
        QStringLiteral("/home/chairman/Games/grand-theft-auto-san-andreas-orig/drive_c/Program Files/Rockstar Games/Grand Theft Auto San Andreas.bak/SAMP/samaps.txd"),
        QStringLiteral("/home/chairman/Projects/sa-mp.dll-rebuild/artifacts/installer_extracted/SAMP/samaps.txd"),
    });
}

QString SaMapLoader::findDefaultHmapPath() {
    return firstExistingPath({
        QStringLiteral("/home/chairman/Projects/sf-cnr/scriptfiles/SAfull.hmap"),
        QStringLiteral("/home/chairman/Projects/omp-server-traffictest/scriptfiles/SAfull.hmap"),
        QStringLiteral("/home/chairman/Projects/Legacy Server 0.3.7/scriptfiles/SAfull.hmap"),
    });
}

QString SaMapLoader::defaultTerrainMapPath() {
    return firstExistingPath({
        QStringLiteral("/home/chairman/Projects/sampplotter/data/San_Andreas_aerial_view.jpg"),
        QStringLiteral("data/San_Andreas_aerial_view.jpg"),
        QStringLiteral("../data/San_Andreas_aerial_view.jpg"),
        QStringLiteral(":/data/San_Andreas_aerial_view.jpg"),
    });
}

bool SaMapLoader::loadBestAvailableMap(const QString& imgPath,
                                       const QString& txdPath,
                                       SaMapAsset* outAsset,
                                       QString* errorMessage) {
    if (!imgPath.isEmpty() && QFileInfo::exists(imgPath)) {
        QString imgError;
        if (loadHighResRadarMap(imgPath, txdPath, outAsset, &imgError)) {
            return true;
        }

        if (txdPath.isEmpty() || !QFileInfo::exists(txdPath)) {
            if (errorMessage) {
                *errorMessage = imgError;
            }
            return false;
        }
    }

    return loadMapFromTxd(txdPath, outAsset, errorMessage);
}

bool SaMapLoader::loadMapFromTxd(const QString& path, SaMapAsset* outAsset, QString* errorMessage) {
    TxdReader reader;
    if (!reader.load(path, errorMessage)) {
        return false;
    }

    QImage fullMap;
    for (const TxdTexture& texture : reader.textures()) {
        if (texture.name == QStringLiteral("map")) {
            fullMap = texture.image;
            break;
        }
    }

    if (fullMap.isNull()) {
        fullMap = composeQuadrants(reader.textures());
    }

    if (fullMap.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not determine a usable map texture from %1").arg(path);
        }
        return false;
    }

    outAsset->image = fullMap;
    outAsset->description = QStringLiteral("Loaded from %1").arg(path);
    return true;
}

bool SaMapLoader::loadTerrainMapImage(const QString& path,
                                      const QString& txdReferencePath,
                                      SaMapAsset* outAsset,
                                      QString* errorMessage) {
    QImage image;
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.readAll();
            image.loadFromData(bytes);
        } else {
            image.load(path);
        }
    }

    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not load terrain map image from %1").arg(path);
        }
        return false;
    }

    QString alignmentText = QStringLiteral("unaligned");
    alignImageToReferenceMap(&image, txdReferencePath, &alignmentText);

    outAsset->image = std::move(image);
    outAsset->description =
        QStringLiteral("%1 via terrain map (%2x%3, %4)")
            .arg(path)
            .arg(outAsset->image.width())
            .arg(outAsset->image.height())
            .arg(alignmentText);
    return true;
}

bool SaMapLoader::dumpTextures(const QString& txdPath, const QString& outputDir, QString* errorMessage) {
    TxdReader reader;
    if (!reader.load(txdPath, errorMessage)) {
        return false;
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create output directory: %1").arg(outputDir);
        }
        return false;
    }

    for (const TxdTexture& texture : reader.textures()) {
        const QString baseName = texture.name.isEmpty() ? QStringLiteral("unnamed") : texture.name;
        const QString targetPath = QDir(outputDir).filePath(baseName + QStringLiteral(".png"));
        if (!texture.image.save(targetPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not save texture to %1").arg(targetPath);
            }
            return false;
        }
    }

    QImage composed = composeQuadrants(reader.textures());
    if (!composed.isNull()) {
        if (!composed.save(QDir(outputDir).filePath(QStringLiteral("composed_quadrants.png")))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not save composed quadrant image");
            }
            return false;
        }
    }

    for (const TxdTexture& texture : reader.textures()) {
        if (texture.name == QStringLiteral("map")) {
            if (!texture.image.save(QDir(outputDir).filePath(QStringLiteral("full_map.png")))) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Could not save full map image");
                }
                return false;
            }
            break;
        }
    }

    return true;
}
