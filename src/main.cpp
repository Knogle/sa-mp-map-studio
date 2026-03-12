#include "main_window.h"
#include "mapandreas_heightmap.h"
#include "sa_map_loader.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>

#include <optional>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/data/samp.ico")));
    QCoreApplication::setApplicationName(QStringLiteral("SA:MP Map Studio"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SA:MP map editor with MapAndreas-compatible XYZ lookup"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption txdOption(QStringList{QStringLiteral("t"), QStringLiteral("txd")},
                                       QStringLiteral("Path to samaps.txd"),
                                       QStringLiteral("path"));
    const QCommandLineOption imgOption(QStringList{QStringLiteral("i"), QStringLiteral("img")},
                                       QStringLiteral("Path to GTA SA models/gta3.img for high-resolution radar tiles"),
                                       QStringLiteral("path"));
    const QCommandLineOption terrainMapOption(QStringList{QStringLiteral("terrain-map")},
                                              QStringLiteral("Path to a terrain/satellite map image"),
                                              QStringLiteral("path"));
    const QCommandLineOption hmapOption(QStringList{QStringLiteral("h"), QStringLiteral("hmap")},
                                        QStringLiteral("Path to SAfull.hmap"),
                                        QStringLiteral("path"));
    const QCommandLineOption dumpOption(QStringList{QStringLiteral("dump-textures")},
                                        QStringLiteral("Dump TXD textures as PNG files into the given directory"),
                                        QStringLiteral("dir"));
    const QCommandLineOption exportOption(QStringList{QStringLiteral("export-map")},
                                          QStringLiteral("Export the selected map image as PNG"),
                                          QStringLiteral("path"));
    const QCommandLineOption noGuiOption(QStringList{QStringLiteral("no-gui")},
                                         QStringLiteral("Do not open the GUI; useful together with export options"));

    parser.addOption(imgOption);
    parser.addOption(terrainMapOption);
    parser.addOption(txdOption);
    parser.addOption(hmapOption);
    parser.addOption(dumpOption);
    parser.addOption(exportOption);
    parser.addOption(noGuiOption);
    parser.process(app);

    const QString imgPath = parser.isSet(imgOption) ? parser.value(imgOption) : SaMapLoader::findDefaultImgPath();
    const QString terrainMapPath =
        parser.isSet(terrainMapOption) ? parser.value(terrainMapOption) : SaMapLoader::defaultTerrainMapPath();
    const QString txdPath = parser.isSet(txdOption) ? parser.value(txdOption) : SaMapLoader::findDefaultTxdPath();
    const QString hmapPath = parser.isSet(hmapOption) ? parser.value(hmapOption) : SaMapLoader::findDefaultHmapPath();

    QString errorMessage;
    if (parser.isSet(dumpOption)) {
        if (txdPath.isEmpty() || !SaMapLoader::dumpTextures(txdPath, parser.value(dumpOption), &errorMessage)) {
            QMessageBox::critical(nullptr, QStringLiteral("SA:MP Map Studio"), errorMessage.isEmpty() ? QStringLiteral("Could not dump TXD textures") : errorMessage);
            return 1;
        }
    }

    SaMapAsset mapAsset;
    if (!SaMapLoader::loadBestAvailableMap(imgPath, txdPath, &mapAsset, &errorMessage)) {
        QMessageBox::critical(nullptr, QStringLiteral("SA:MP Map Studio"), errorMessage.isEmpty() ? QStringLiteral("Could not load SA map") : errorMessage);
        return 1;
    }

    std::optional<SaMapAsset> terrainMapAsset;
    if (!terrainMapPath.isEmpty()) {
        SaMapAsset loadedTerrainMap;
        QString terrainError;
        if (SaMapLoader::loadTerrainMapImage(terrainMapPath, txdPath, &loadedTerrainMap, &terrainError)) {
            terrainMapAsset = std::move(loadedTerrainMap);
        }
    }

    if (parser.isSet(exportOption) && !mapAsset.image.save(parser.value(exportOption))) {
        QMessageBox::critical(nullptr, QStringLiteral("SA:MP Map Studio"), QStringLiteral("Could not export map image to %1").arg(parser.value(exportOption)));
        return 1;
    }

    if (parser.isSet(noGuiOption)) {
        return 0;
    }

    MapAndreasHeightMap heightMap;
    if (!hmapPath.isEmpty()) {
        QString heightError;
        if (!heightMap.load(hmapPath, &heightError)) {
            QMessageBox::critical(nullptr, QStringLiteral("SA:MP Map Studio"), heightError);
            return 1;
        }
    }

    MainWindow window(std::move(mapAsset), std::move(terrainMapAsset), std::move(heightMap), hmapPath);
    window.show();
    return app.exec();
}
