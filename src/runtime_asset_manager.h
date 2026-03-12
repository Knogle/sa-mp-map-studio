#pragma once

#include "mapandreas_heightmap.h"
#include "sa_map_loader.h"

#include <QImage>
#include <QString>

#include <optional>

struct RuntimeAssetBundle {
    SaMapAsset radarMapAsset;
    std::optional<SaMapAsset> terrainMapAsset;
    MapAndreasHeightMap heightMap;
    QString imgPath;
    QString terrainMapPath;
    QString hmapPath;
    QString colAndreasPath;
    QString cacheRootPath;
    QString runtimeNotes;
};

class RuntimeAssetManager {
public:
    static QString cacheRootPath();
    static QString managedMapAndreasHeightmapPath();
    static QString managedColAndreasDatabasePath();

    static QString configuredImgPath();
    static void setConfiguredImgPath(const QString& path);

    static bool downloadMapAndreasHeightmap(QString* downloadedPath = nullptr, QString* errorMessage = nullptr);
    static bool loadRuntimeBundle(const QString& imgPathOverride,
                                  const QString& txdPath,
                                  const QString& terrainMapPathOverride,
                                  const QString& hmapPathOverride,
                                  RuntimeAssetBundle* outBundle,
                                  QString* errorMessage = nullptr,
                                  bool allowDownload = true);
};
