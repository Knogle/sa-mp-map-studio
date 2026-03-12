#pragma once

#include "txd_reader.h"

#include <QImage>
#include <QString>

struct SaMapAsset {
    QImage image;
    QString description;
};

class SaMapLoader {
public:
    static QString findDefaultImgPath();
    static QString findDefaultTxdPath();
    static QString findDefaultHmapPath();
    static QString defaultTerrainMapPath();

    static bool loadBestAvailableMap(const QString& imgPath,
                                     const QString& txdPath,
                                     SaMapAsset* outAsset,
                                     QString* errorMessage = nullptr);
    static bool loadTerrainMapImage(const QString& path,
                                    const QImage& referenceMap,
                                    SaMapAsset* outAsset,
                                    QString* errorMessage = nullptr);
    static bool loadTerrainMapImage(const QString& path,
                                    const QString& txdReferencePath,
                                    SaMapAsset* outAsset,
                                    QString* errorMessage = nullptr);
    static bool loadMapFromTxd(const QString& path, SaMapAsset* outAsset, QString* errorMessage = nullptr);
    static bool dumpTextures(const QString& txdPath, const QString& outputDir, QString* errorMessage = nullptr);
};
