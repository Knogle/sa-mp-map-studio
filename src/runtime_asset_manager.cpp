#include "runtime_asset_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString ensureDirectory(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.absolutePath();
}

QString firstExistingPath(const QStringList& candidates) {
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

bool downloadFile(const QList<QUrl>& urls, const QString& targetPath, QString* successfulUrl, QString* errorMessage) {
    QNetworkAccessManager manager;

    for (const QUrl& url : urls) {
        if (!url.isValid()) {
            continue;
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QEventLoop loop;
        QNetworkReply* reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const auto cleanupReply = [&reply]() {
            if (reply) {
                reply->deleteLater();
                reply = nullptr;
            }
        };

        if (reply->error() != QNetworkReply::NoError) {
            if (errorMessage) {
                *errorMessage = reply->errorString();
            }
            cleanupReply();
            continue;
        }

        const QByteArray payload = reply->readAll();
        cleanupReply();

        if (payload.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Received an empty response from %1").arg(url.toString());
            }
            continue;
        }

        QSaveFile file(targetPath);
        if (!file.open(QIODevice::WriteOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not open %1 for writing").arg(targetPath);
            }
            return false;
        }

        if (file.write(payload) != payload.size() || !file.commit()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not write downloaded file to %1").arg(targetPath);
            }
            return false;
        }

        if (successfulUrl) {
            *successfulUrl = url.toString();
        }
        return true;
    }

    if (errorMessage && errorMessage->isEmpty()) {
        *errorMessage = QStringLiteral("Could not download from any configured URL");
    }
    return false;
}

QString settingsGroupName() {
    return QStringLiteral("runtime");
}

QString chooseImgPath(const QString& overridePath) {
    if (!overridePath.trimmed().isEmpty()) {
        return overridePath.trimmed();
    }

    const QString configured = RuntimeAssetManager::configuredImgPath();
    if (!configured.isEmpty()) {
        return configured;
    }

    return SaMapLoader::findDefaultImgPath();
}

QString chooseTerrainPath(const QString& overridePath) {
    if (!overridePath.trimmed().isEmpty()) {
        return overridePath.trimmed();
    }
    return SaMapLoader::defaultTerrainMapPath();
}

} // namespace

QString RuntimeAssetManager::cacheRootPath() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).isEmpty()
            ? QDir::home().filePath(QStringLiteral(".local/share/sa-mp-map-studio"))
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return ensureDirectory(QDir(base).filePath(QStringLiteral("runtime-assets")));
}

QString RuntimeAssetManager::managedMapAndreasHeightmapPath() {
    return QDir(ensureDirectory(QDir(cacheRootPath()).filePath(QStringLiteral("mapandreas"))))
        .filePath(QStringLiteral("SAfull.hmap"));
}

QString RuntimeAssetManager::managedColAndreasDatabasePath() {
    return QDir(ensureDirectory(QDir(cacheRootPath()).filePath(QStringLiteral("colandreas"))))
        .filePath(QStringLiteral("ColAndreas.cadb"));
}

QString RuntimeAssetManager::configuredImgPath() {
    QSettings settings;
    settings.beginGroup(settingsGroupName());
    const QString value = settings.value(QStringLiteral("gta3_img_path")).toString().trimmed();
    settings.endGroup();
    return value;
}

void RuntimeAssetManager::setConfiguredImgPath(const QString& path) {
    QSettings settings;
    settings.beginGroup(settingsGroupName());
    if (path.trimmed().isEmpty()) {
        settings.remove(QStringLiteral("gta3_img_path"));
    } else {
        settings.setValue(QStringLiteral("gta3_img_path"), path.trimmed());
    }
    settings.endGroup();
}

bool RuntimeAssetManager::downloadMapAndreasHeightmap(QString* downloadedPath, QString* errorMessage) {
    const QString targetPath = managedMapAndreasHeightmapPath();
    QFileInfo targetInfo(targetPath);
    ensureDirectory(targetInfo.dir().absolutePath());

    const QList<QUrl> urls = {
        QUrl(QStringLiteral("https://raw.githubusercontent.com/philip1337/samp-plugin-mapandreas/master/heightmaps/SAfull.hmap")),
        QUrl(QStringLiteral("https://raw.githubusercontent.com/philip1337/samp-plugin-mapandreas/master/heightmaps/SAFull.hmap")),
    };

    QString resolvedUrl;
    QString downloadError;
    if (!downloadFile(urls, targetPath, &resolvedUrl, &downloadError)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not download MapAndreas heightmap: %1").arg(downloadError);
        }
        return false;
    }

    QFile file(targetPath);
    if (file.size() != static_cast<qint64>(MapAndreasHeightMap::kPointCount) * static_cast<qint64>(sizeof(unsigned short))) {
        file.remove();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Downloaded MapAndreas file from %1 has an unexpected size").arg(resolvedUrl);
        }
        return false;
    }

    if (downloadedPath) {
        *downloadedPath = targetPath;
    }
    return true;
}

bool RuntimeAssetManager::loadRuntimeBundle(const QString& imgPathOverride,
                                            const QString& txdPath,
                                            const QString& terrainMapPathOverride,
                                            const QString& hmapPathOverride,
                                            RuntimeAssetBundle* outBundle,
                                            QString* errorMessage,
                                            bool allowDownload) {
    if (!outBundle) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No runtime bundle output target was provided");
        }
        return false;
    }

    RuntimeAssetBundle bundle;
    bundle.cacheRootPath = cacheRootPath();
    bundle.colAndreasPath = managedColAndreasDatabasePath();
    bundle.imgPath = chooseImgPath(imgPathOverride);
    bundle.terrainMapPath = chooseTerrainPath(terrainMapPathOverride);

    QString mapError;
    if (!SaMapLoader::loadBestAvailableMap(bundle.imgPath, txdPath, &bundle.radarMapAsset, &mapError)) {
        QString terrainFallbackError;
        if (!SaMapLoader::loadTerrainMapImage(bundle.terrainMapPath, QImage(), &bundle.radarMapAsset, &terrainFallbackError)) {
            if (errorMessage) {
                *errorMessage =
                    mapError.isEmpty() ? terrainFallbackError
                                       : QStringLiteral("%1\n%2").arg(mapError, terrainFallbackError);
            }
            return false;
        }

        bundle.runtimeNotes += QStringLiteral("Radar map fallback: bundled terrain map is used because no valid gta3.img/samaps.txd source was available.\n");
    }

    if (!bundle.terrainMapPath.isEmpty()) {
        SaMapAsset terrainAsset;
        QString terrainError;
        if (SaMapLoader::loadTerrainMapImage(bundle.terrainMapPath, bundle.radarMapAsset.image, &terrainAsset, &terrainError)) {
            bundle.terrainMapAsset = std::move(terrainAsset);
        } else {
            bundle.runtimeNotes += QStringLiteral("Terrain map unavailable: %1\n").arg(terrainError);
        }
    }

    QString resolvedHmapPath;
    if (!hmapPathOverride.trimmed().isEmpty()) {
        resolvedHmapPath = hmapPathOverride.trimmed();
    } else {
        resolvedHmapPath = firstExistingPath({
            managedMapAndreasHeightmapPath(),
            SaMapLoader::findDefaultHmapPath(),
        });
    }

    if (resolvedHmapPath.isEmpty() && allowDownload) {
        QString downloadedPath;
        QString downloadError;
        if (downloadMapAndreasHeightmap(&downloadedPath, &downloadError)) {
            resolvedHmapPath = downloadedPath;
            bundle.runtimeNotes += QStringLiteral("Downloaded MapAndreas heightmap into the local runtime cache.\n");
        } else {
            bundle.runtimeNotes += QStringLiteral("MapAndreas download failed: %1\n").arg(downloadError);
        }
    }

    bundle.hmapPath = resolvedHmapPath;
    if (!bundle.hmapPath.isEmpty()) {
        QString heightError;
        if (!bundle.heightMap.load(bundle.hmapPath, &heightError)) {
            if (!hmapPathOverride.trimmed().isEmpty()) {
                if (errorMessage) {
                    *errorMessage = heightError;
                }
                return false;
            }
            bundle.runtimeNotes += QStringLiteral("Height lookup unavailable: %1\n").arg(heightError);
            bundle.hmapPath.clear();
        }
    } else {
        bundle.runtimeNotes += QStringLiteral("Height lookup unavailable: no MapAndreas heightmap was found locally.\n");
    }

    *outBundle = std::move(bundle);
    return true;
}
