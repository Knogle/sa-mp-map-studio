#include "main_window.h"

#include "map_widget.h"
#include "runtime_asset_manager.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

QString formatFloat(double value, int precision = 4) {
    return QString::number(value, 'f', precision);
}

QString colorToPawn(const QColor& color) {
    const QColor rgba = color.isValid() ? color : QColor(102, 204, 255, 102);
    return QStringLiteral("0x%1%2%3%4")
        .arg(rgba.alpha(), 2, 16, QLatin1Char('0'))
        .arg(rgba.red(), 2, 16, QLatin1Char('0'))
        .arg(rgba.green(), 2, 16, QLatin1Char('0'))
        .arg(rgba.blue(), 2, 16, QLatin1Char('0'))
        .toUpper();
}

QColor parsePawnColor(QString text, const QColor& fallback) {
    text = text.trimmed();
    if (text.isEmpty()) {
        return fallback;
    }

    bool ok = false;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        const quint32 argb = text.mid(2).toUInt(&ok, 16);
        if (ok && text.size() == 10) {
            return QColor((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
        }
    }

    QColor color(text);
    if (color.isValid()) {
        return color;
    }

    return fallback;
}

QString sanitizeIdentifier(QString text) {
    text = text.toLower();
    for (QChar& ch : text) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }
    while (text.contains(QStringLiteral("__"))) {
        text.replace(QStringLiteral("__"), QStringLiteral("_"));
    }
    text.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (text.isEmpty()) {
        text = QStringLiteral("item");
    }
    if (text.front().isDigit()) {
        text.prepend(QStringLiteral("item_"));
    }
    return text;
}

} // namespace

MainWindow::MainWindow(RuntimeAssetBundle runtimeBundle,
                       QString txdPath,
                       QString terrainMapPath,
                       QWidget* parent)
    : QMainWindow(parent),
      heightMap_(std::move(runtimeBundle.heightMap)),
      zoneDatabase_(),
      radarMapAsset_(std::move(runtimeBundle.radarMapAsset)),
      terrainMapAsset_(std::move(runtimeBundle.terrainMapAsset)),
      imgPath_(std::move(runtimeBundle.imgPath)),
      hmapPath_(std::move(runtimeBundle.hmapPath)),
      txdPath_(std::move(txdPath)),
      terrainMapPath_(std::move(terrainMapPath)),
      cacheRootPath_(std::move(runtimeBundle.cacheRootPath)),
      colAndreasPath_(std::move(runtimeBundle.colAndreasPath)),
      runtimeNotes_(std::move(runtimeBundle.runtimeNotes)) {
    setWindowTitle(QStringLiteral("SA:MP Map Studio"));
    resize(1920, 1080);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    rootLayout->addWidget(splitter, 1);

    auto* mapPane = new QWidget(splitter);
    auto* mapPaneLayout = new QVBoxLayout(mapPane);
    mapPaneLayout->setContentsMargins(0, 0, 0, 0);
    mapPaneLayout->setSpacing(10);

    mapWidget_ = new MapWidget(mapPane);
    mapWidget_->setMapImage(radarMapAsset_.image);
    mapPaneLayout->addWidget(mapWidget_, 1);

    auto* infoLayout = new QHBoxLayout();
    infoLayout->setSpacing(12);

    xyzLabel_ = new QLabel(QStringLiteral("Left click to inspect or place an item"), mapPane);
    xyzLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(xyzLabel_, 1);

    mapModeCombo_ = new QComboBox(mapPane);
    rebuildMapModeCombo();
    infoLayout->addWidget(mapModeCombo_);

    zoomOutButton_ = new QPushButton(QStringLiteral("-"), mapPane);
    infoLayout->addWidget(zoomOutButton_);

    zoomInButton_ = new QPushButton(QStringLiteral("+"), mapPane);
    infoLayout->addWidget(zoomInButton_);

    resetZoomButton_ = new QPushButton(QStringLiteral("Reset Zoom"), mapPane);
    infoLayout->addWidget(resetZoomButton_);

    zoomLabel_ = new QLabel(mapPane);
    zoomLabel_->setMinimumWidth(90);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    infoLayout->addWidget(zoomLabel_);

    copyButton_ = new QPushButton(QStringLiteral("Copy XYZ"), mapPane);
    infoLayout->addWidget(copyButton_);

    mapPaneLayout->addLayout(infoLayout);

    hoverLabel_ = new QLabel(QStringLiteral("Hover: move over the map to see the SA:MP zone"), mapPane);
    hoverLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mapPaneLayout->addWidget(hoverLabel_);

    sourceLabel_ = new QLabel(mapPane);
    sourceLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mapPaneLayout->addWidget(sourceLabel_);

    auto* rightTabs = new QTabWidget(splitter);

    auto* placementTab = new QWidget(rightTabs);
    auto* placementLayout = new QVBoxLayout(placementTab);
    placementLayout->setContentsMargins(12, 12, 12, 12);
    placementLayout->setSpacing(12);

    auto* modeGroup = new QGroupBox(QStringLiteral("Mode"), placementTab);
    auto* modeLayout = new QVBoxLayout(modeGroup);
    toolModeCombo_ = new QComboBox(modeGroup);
    toolModeCombo_->addItem(QStringLiteral("Inspect"), static_cast<int>(EditorToolMode::Inspect));
    toolModeCombo_->addItem(QStringLiteral("Place Vehicle"), static_cast<int>(EditorToolMode::Vehicle));
    toolModeCombo_->addItem(QStringLiteral("Set Player Spawn"), static_cast<int>(EditorToolMode::PlayerSpawn));
    toolModeCombo_->addItem(QStringLiteral("Define Gang Zone"), static_cast<int>(EditorToolMode::GangZone));
    modeLayout->addWidget(toolModeCombo_);
    toolHintLabel_ = new QLabel(modeGroup);
    toolHintLabel_->setWordWrap(true);
    modeLayout->addWidget(toolHintLabel_);
    placementLayout->addWidget(modeGroup);

    auto* settingsGroup = new QGroupBox(QStringLiteral("Tool Settings"), placementTab);
    auto* settingsLayout = new QVBoxLayout(settingsGroup);
    toolSettingsStack_ = new QStackedWidget(settingsGroup);
    settingsLayout->addWidget(toolSettingsStack_);

    {
        auto* inspectPage = new QWidget(toolSettingsStack_);
        auto* inspectLayout = new QVBoxLayout(inspectPage);
        auto* inspectLabel = new QLabel(QStringLiteral("Inspect mode only reads coordinates and SA:MP zones. It does not create items."), inspectPage);
        inspectLabel->setWordWrap(true);
        inspectLayout->addWidget(inspectLabel);
        inspectLayout->addStretch(1);
        toolSettingsStack_->addWidget(inspectPage);
    }

    {
        auto* vehiclePage = new QWidget(toolSettingsStack_);
        auto* vehicleLayout = new QFormLayout(vehiclePage);
        vehicleNameEdit_ = new QLineEdit(vehiclePage);
        vehicleModelSpin_ = new QSpinBox(vehiclePage);
        vehicleModelSpin_->setRange(400, 611);
        vehicleModelSpin_->setValue(411);
        vehicleAngleSpin_ = new QDoubleSpinBox(vehiclePage);
        vehicleAngleSpin_->setRange(0.0, 360.0);
        vehicleAngleSpin_->setDecimals(3);
        vehicleColor1Spin_ = new QSpinBox(vehiclePage);
        vehicleColor1Spin_->setRange(-1, 255);
        vehicleColor1Spin_->setValue(-1);
        vehicleColor2Spin_ = new QSpinBox(vehiclePage);
        vehicleColor2Spin_->setRange(-1, 255);
        vehicleColor2Spin_->setValue(-1);
        vehicleRespawnSpin_ = new QSpinBox(vehiclePage);
        vehicleRespawnSpin_->setRange(0, 1000000);
        vehicleRespawnSpin_->setValue(120);
        vehicleLayout->addRow(QStringLiteral("Name"), vehicleNameEdit_);
        vehicleLayout->addRow(QStringLiteral("Model ID"), vehicleModelSpin_);
        vehicleLayout->addRow(QStringLiteral("Angle"), vehicleAngleSpin_);
        vehicleLayout->addRow(QStringLiteral("Color 1"), vehicleColor1Spin_);
        vehicleLayout->addRow(QStringLiteral("Color 2"), vehicleColor2Spin_);
        vehicleLayout->addRow(QStringLiteral("Respawn s"), vehicleRespawnSpin_);
        toolSettingsStack_->addWidget(vehiclePage);
    }

    {
        auto* spawnPage = new QWidget(toolSettingsStack_);
        auto* spawnLayout = new QFormLayout(spawnPage);
        spawnNameEdit_ = new QLineEdit(spawnPage);
        spawnSkinSpin_ = new QSpinBox(spawnPage);
        spawnSkinSpin_->setRange(0, 311);
        spawnTeamSpin_ = new QSpinBox(spawnPage);
        spawnTeamSpin_->setRange(0, 255);
        spawnAngleSpin_ = new QDoubleSpinBox(spawnPage);
        spawnAngleSpin_->setRange(0.0, 360.0);
        spawnAngleSpin_->setDecimals(3);
        spawnWeapon1Spin_ = new QSpinBox(spawnPage);
        spawnWeapon1Spin_->setRange(0, 46);
        spawnAmmo1Spin_ = new QSpinBox(spawnPage);
        spawnAmmo1Spin_->setRange(0, 9999);
        spawnWeapon2Spin_ = new QSpinBox(spawnPage);
        spawnWeapon2Spin_->setRange(0, 46);
        spawnAmmo2Spin_ = new QSpinBox(spawnPage);
        spawnAmmo2Spin_->setRange(0, 9999);
        spawnWeapon3Spin_ = new QSpinBox(spawnPage);
        spawnWeapon3Spin_->setRange(0, 46);
        spawnAmmo3Spin_ = new QSpinBox(spawnPage);
        spawnAmmo3Spin_->setRange(0, 9999);
        spawnLayout->addRow(QStringLiteral("Name"), spawnNameEdit_);
        spawnLayout->addRow(QStringLiteral("Skin"), spawnSkinSpin_);
        spawnLayout->addRow(QStringLiteral("Team"), spawnTeamSpin_);
        spawnLayout->addRow(QStringLiteral("Angle"), spawnAngleSpin_);
        spawnLayout->addRow(QStringLiteral("Weapon 1"), spawnWeapon1Spin_);
        spawnLayout->addRow(QStringLiteral("Ammo 1"), spawnAmmo1Spin_);
        spawnLayout->addRow(QStringLiteral("Weapon 2"), spawnWeapon2Spin_);
        spawnLayout->addRow(QStringLiteral("Ammo 2"), spawnAmmo2Spin_);
        spawnLayout->addRow(QStringLiteral("Weapon 3"), spawnWeapon3Spin_);
        spawnLayout->addRow(QStringLiteral("Ammo 3"), spawnAmmo3Spin_);
        toolSettingsStack_->addWidget(spawnPage);
    }

    {
        auto* zonePage = new QWidget(toolSettingsStack_);
        auto* zoneLayout = new QFormLayout(zonePage);
        gangZoneNameEdit_ = new QLineEdit(zonePage);
        gangZoneColorEdit_ = new QLineEdit(QStringLiteral("0x66CCFFFF"), zonePage);
        auto* zoneHelp = new QLabel(QStringLiteral("Drag with the left mouse button on the map to create a rectangular gang zone."), zonePage);
        zoneHelp->setWordWrap(true);
        zoneLayout->addRow(QStringLiteral("Name"), gangZoneNameEdit_);
        zoneLayout->addRow(QStringLiteral("Color"), gangZoneColorEdit_);
        zoneLayout->addRow(zoneHelp);
        toolSettingsStack_->addWidget(zonePage);
    }

    placementLayout->addWidget(settingsGroup);

    auto* itemsGroup = new QGroupBox(QStringLiteral("Placed Items"), placementTab);
    auto* itemsLayout = new QVBoxLayout(itemsGroup);
    itemTable_ = new QTableWidget(0, 4, itemsGroup);
    itemTable_->setHorizontalHeaderLabels({QStringLiteral("Type"), QStringLiteral("Name"), QStringLiteral("Zone"), QStringLiteral("Data")});
    itemTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    itemTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    itemTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    itemTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    itemTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    itemTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    itemTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    itemTable_->setAlternatingRowColors(true);
    itemsLayout->addWidget(itemTable_, 1);

    auto* itemButtonsLayout = new QHBoxLayout();
    copySelectedPawnButton_ = new QPushButton(QStringLiteral("Copy Selected Pawn"), itemsGroup);
    removeItemButton_ = new QPushButton(QStringLiteral("Remove Selected"), itemsGroup);
    clearItemsButton_ = new QPushButton(QStringLiteral("Clear All"), itemsGroup);
    itemButtonsLayout->addWidget(copySelectedPawnButton_);
    itemButtonsLayout->addWidget(removeItemButton_);
    itemButtonsLayout->addWidget(clearItemsButton_);
    itemsLayout->addLayout(itemButtonsLayout);

    placementLayout->addWidget(itemsGroup, 1);
    rightTabs->addTab(placementTab, QStringLiteral("Placement"));

    auto* sourcesTab = new QWidget(rightTabs);
    auto* sourcesLayout = new QVBoxLayout(sourcesTab);
    sourcesLayout->setContentsMargins(12, 12, 12, 12);
    sourcesLayout->setSpacing(12);

    auto* gtaGroup = new QGroupBox(QStringLiteral("GTA SA Sources"), sourcesTab);
    auto* gtaLayout = new QFormLayout(gtaGroup);
    imgPathEdit_ = new QLineEdit(imgPath_, gtaGroup);
    browseImgButton_ = new QPushButton(QStringLiteral("Browse..."), gtaGroup);
    auto* imgRow = new QHBoxLayout();
    imgRow->addWidget(imgPathEdit_, 1);
    imgRow->addWidget(browseImgButton_);
    gtaLayout->addRow(QStringLiteral("gta3.img"), imgRow);
    auto* gtaHelp = new QLabel(QStringLiteral("Point this to GTA San Andreas `models/gta3.img`. The app extracts the radar tiles from there and keeps the configured path in local settings."), gtaGroup);
    gtaHelp->setWordWrap(true);
    gtaLayout->addRow(gtaHelp);
    sourcesLayout->addWidget(gtaGroup);

    auto* runtimeGroup = new QGroupBox(QStringLiteral("Managed Runtime Data"), sourcesTab);
    auto* runtimeLayout = new QVBoxLayout(runtimeGroup);
    cachePathLabel_ = new QLabel(runtimeGroup);
    cachePathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    runtimeLayout->addWidget(cachePathLabel_);
    heightmapStatusLabel_ = new QLabel(runtimeGroup);
    heightmapStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    runtimeLayout->addWidget(heightmapStatusLabel_);
    colAndreasStatusLabel_ = new QLabel(runtimeGroup);
    colAndreasStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    runtimeLayout->addWidget(colAndreasStatusLabel_);
    auto* runtimeButtons = new QHBoxLayout();
    reloadSourcesButton_ = new QPushButton(QStringLiteral("Reload Sources"), runtimeGroup);
    downloadHeightmapButton_ = new QPushButton(QStringLiteral("Download MapAndreas"), runtimeGroup);
    runtimeButtons->addWidget(reloadSourcesButton_);
    runtimeButtons->addWidget(downloadHeightmapButton_);
    runtimeButtons->addStretch(1);
    runtimeLayout->addLayout(runtimeButtons);
    runtimeNotesEdit_ = new QPlainTextEdit(runtimeGroup);
    runtimeNotesEdit_->setReadOnly(true);
    runtimeNotesEdit_->setMaximumBlockCount(200);
    runtimeLayout->addWidget(runtimeNotesEdit_, 1);
    sourcesLayout->addWidget(runtimeGroup, 1);
    rightTabs->addTab(sourcesTab, QStringLiteral("Sources"));

    auto* exportTab = new QWidget(rightTabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    exportLayout->setContentsMargins(12, 12, 12, 12);
    exportLayout->setSpacing(12);

    auto* exportControlsLayout = new QHBoxLayout();
    exportFormatCombo_ = new QComboBox(exportTab);
    exportFormatCombo_->addItem(QStringLiteral("Pawn"), QStringLiteral("pawn"));
    exportFormatCombo_->addItem(QStringLiteral("JSON"), QStringLiteral("json"));
    exportControlsLayout->addWidget(exportFormatCombo_);
    copyExportButton_ = new QPushButton(QStringLiteral("Copy Export"), exportTab);
    exportControlsLayout->addWidget(copyExportButton_);
    saveExportButton_ = new QPushButton(QStringLiteral("Save Export"), exportTab);
    exportControlsLayout->addWidget(saveExportButton_);
    exportControlsLayout->addStretch(1);
    exportLayout->addLayout(exportControlsLayout);

    exportPreview_ = new QPlainTextEdit(exportTab);
    exportPreview_->setReadOnly(true);
    exportLayout->addWidget(exportPreview_, 1);
    rightTabs->addTab(exportTab, QStringLiteral("Export"));

    splitter->addWidget(mapPane);
    splitter->addWidget(rightTabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({1450, 470});

    setCentralWidget(central);

    connect(mapWidget_, &MapWidget::mapClicked, this, &MainWindow::handleMapClicked);
    connect(mapWidget_, &MapWidget::mapHovered, this, &MainWindow::handleMapHovered);
    connect(mapWidget_, &MapWidget::rectangleSelectionFinished, this, &MainWindow::handleRectangleSelectionFinished);
    connect(mapWidget_, &MapWidget::zoomChanged, this, &MainWindow::updateZoomLabel);
    connect(copyButton_, &QPushButton::clicked, this, &MainWindow::copyCurrentCoordinates);
    connect(mapModeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::handleMapModeChanged);
    connect(zoomInButton_, &QPushButton::clicked, mapWidget_, &MapWidget::zoomIn);
    connect(zoomOutButton_, &QPushButton::clicked, mapWidget_, &MapWidget::zoomOut);
    connect(resetZoomButton_, &QPushButton::clicked, mapWidget_, &MapWidget::resetZoom);
    connect(toolModeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::updateToolModeUi);
    connect(itemTable_, &QTableWidget::itemSelectionChanged, this, &MainWindow::handleItemSelectionChanged);
    connect(removeItemButton_, &QPushButton::clicked, this, &MainWindow::deleteSelectedItem);
    connect(clearItemsButton_, &QPushButton::clicked, this, &MainWindow::clearItems);
    connect(copySelectedPawnButton_, &QPushButton::clicked, this, &MainWindow::copySelectedItemPawn);
    connect(exportFormatCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::updateExportPreview);
    connect(copyExportButton_, &QPushButton::clicked, this, &MainWindow::copyExport);
    connect(saveExportButton_, &QPushButton::clicked, this, &MainWindow::saveExport);
    connect(browseImgButton_, &QPushButton::clicked, this, &MainWindow::browseImgPath);
    connect(reloadSourcesButton_, &QPushButton::clicked, this, &MainWindow::reloadRuntimeSources);
    connect(downloadHeightmapButton_, &QPushButton::clicked, this, &MainWindow::downloadManagedHeightmap);

    updateZoomLabel(mapWidget_->zoomFactor());
    updateSourceLabel();
    updateToolModeUi();
    updateExportPreview();
}

EditorToolMode MainWindow::currentToolMode() const {
    return static_cast<EditorToolMode>(toolModeCombo_->currentData().toInt());
}

std::optional<double> MainWindow::lookupZ(double worldX, double worldY) const {
    return heightMap_.lookupZ(worldX, worldY);
}

QString MainWindow::lookupZoneName(double worldX, double worldY, std::optional<double> z) const {
    if (!zoneDatabase_.isLoaded()) {
        return {};
    }
    return zoneDatabase_.lookup(worldX, worldY, z);
}

QString MainWindow::defaultItemName(EditorItemType type) const {
    int count = 0;
    for (const EditorItem& item : items_) {
        if (item.type == type) {
            ++count;
        }
    }
    return QStringLiteral("%1 %2").arg(editorItemTypeName(type)).arg(count + 1);
}

int MainWindow::selectedItemId() const {
    const int row = itemTable_->currentRow();
    if (row < 0) {
        return -1;
    }
    const QTableWidgetItem* item = itemTable_->item(row, 0);
    if (!item) {
        return -1;
    }
    return item->data(Qt::UserRole).toInt();
}

const EditorItem* MainWindow::findItemById(int id) const {
    const auto it = std::find_if(items_.begin(), items_.end(), [id](const EditorItem& item) { return item.id == id; });
    return it == items_.end() ? nullptr : &(*it);
}

QColor MainWindow::currentGangZoneColor() const {
    return parsePawnColor(gangZoneColorEdit_->text(), QColor(102, 204, 255, 102));
}

QString MainWindow::itemSummary(const EditorItem& item) const {
    if (item.type == EditorItemType::GangZone) {
        return QStringLiteral("%1, %2 -> %3, %4")
            .arg(formatFloat(item.rectWorld.left(), 1))
            .arg(formatFloat(item.rectWorld.top(), 1))
            .arg(formatFloat(item.rectWorld.right(), 1))
            .arg(formatFloat(item.rectWorld.bottom(), 1));
    }

    return QStringLiteral("%1, %2, %3 @ %4°")
        .arg(formatFloat(item.pointWorld.x(), 1))
        .arg(formatFloat(item.pointWorld.y(), 1))
        .arg(formatFloat(item.z, 1))
        .arg(formatFloat(item.angle, 1));
}

QString MainWindow::generatePawnForItem(const EditorItem& item, int ordinal) const {
    const QString commentZone = item.zoneName.isEmpty() ? QString() : QStringLiteral(" - %1").arg(item.zoneName);
    if (item.type == EditorItemType::Vehicle) {
        return QStringLiteral("// %1%2\nCreateVehicle(%3, %4, %5, %6, %7, %8, %9, %10);")
            .arg(item.name)
            .arg(commentZone)
            .arg(item.modelId)
            .arg(formatFloat(item.pointWorld.x()))
            .arg(formatFloat(item.pointWorld.y()))
            .arg(formatFloat(item.z))
            .arg(formatFloat(item.angle))
            .arg(item.color1)
            .arg(item.color2)
            .arg(item.respawnDelay);
    }

    if (item.type == EditorItemType::PlayerSpawn) {
        return QStringLiteral("// %1%2\nAddPlayerClassEx(%3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15);")
            .arg(item.name)
            .arg(commentZone)
            .arg(item.team)
            .arg(item.skin)
            .arg(formatFloat(item.pointWorld.x()))
            .arg(formatFloat(item.pointWorld.y()))
            .arg(formatFloat(item.z))
            .arg(formatFloat(item.angle))
            .arg(item.weapons[0])
            .arg(item.ammo[0])
            .arg(item.weapons[1])
            .arg(item.ammo[1])
            .arg(item.weapons[2])
            .arg(item.ammo[2]);
    }

    const QString variableName = QStringLiteral("zone_%1_%2").arg(ordinal).arg(sanitizeIdentifier(item.name));
    return QStringLiteral("// %1%2\nnew %3 = GangZoneCreate(%4, %5, %6, %7);\nGangZoneShowForAll(%3, %8);")
        .arg(item.name)
        .arg(commentZone)
        .arg(variableName)
        .arg(formatFloat(item.rectWorld.left()))
        .arg(formatFloat(item.rectWorld.top()))
        .arg(formatFloat(item.rectWorld.right()))
        .arg(formatFloat(item.rectWorld.bottom()))
        .arg(colorToPawn(item.displayColor));
}

QString MainWindow::generatePawnExport() const {
    QStringList lines;
    lines << QStringLiteral("// Generated by SA:MP Map Studio");
    if (items_.isEmpty()) {
        lines << QStringLiteral("// No items placed yet.");
        return lines.join('\n');
    }

    for (int i = 0; i < items_.size(); ++i) {
        if (i > 0) {
            lines << QString();
        }
        lines << generatePawnForItem(items_[i], i + 1);
    }
    return lines.join('\n');
}

QString MainWindow::generateJsonExport() const {
    QJsonArray array;
    for (const EditorItem& item : items_) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), item.id);
        object.insert(QStringLiteral("type"), editorItemTypeName(item.type));
        object.insert(QStringLiteral("name"), item.name);
        object.insert(QStringLiteral("zone"), item.zoneName);
        object.insert(QStringLiteral("angle"), item.angle);
        object.insert(QStringLiteral("color"), colorToPawn(item.displayColor));

        if (item.type == EditorItemType::GangZone) {
            object.insert(QStringLiteral("minX"), item.rectWorld.left());
            object.insert(QStringLiteral("minY"), item.rectWorld.top());
            object.insert(QStringLiteral("maxX"), item.rectWorld.right());
            object.insert(QStringLiteral("maxY"), item.rectWorld.bottom());
        } else {
            object.insert(QStringLiteral("x"), item.pointWorld.x());
            object.insert(QStringLiteral("y"), item.pointWorld.y());
            object.insert(QStringLiteral("z"), item.z);
        }

        if (item.type == EditorItemType::Vehicle) {
            object.insert(QStringLiteral("modelId"), item.modelId);
            object.insert(QStringLiteral("color1"), item.color1);
            object.insert(QStringLiteral("color2"), item.color2);
            object.insert(QStringLiteral("respawnDelay"), item.respawnDelay);
        }

        if (item.type == EditorItemType::PlayerSpawn) {
            object.insert(QStringLiteral("team"), item.team);
            object.insert(QStringLiteral("skin"), item.skin);
            QJsonArray weapons;
            QJsonArray ammo;
            for (int weapon : item.weapons) {
                weapons.append(weapon);
            }
            for (int ammoCount : item.ammo) {
                ammo.append(ammoCount);
            }
            object.insert(QStringLiteral("weapons"), weapons);
            object.insert(QStringLiteral("ammo"), ammo);
        }

        array.append(object);
    }

    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

const SaMapAsset& MainWindow::currentMapAsset() const {
    if (mapModeCombo_ && mapModeCombo_->currentIndex() == 1 && terrainMapAsset_.has_value()) {
        return *terrainMapAsset_;
    }
    return radarMapAsset_;
}

void MainWindow::applyRuntimeBundle(RuntimeAssetBundle bundle) {
    radarMapAsset_ = std::move(bundle.radarMapAsset);
    terrainMapAsset_ = std::move(bundle.terrainMapAsset);
    heightMap_ = std::move(bundle.heightMap);
    imgPath_ = std::move(bundle.imgPath);
    hmapPath_ = std::move(bundle.hmapPath);
    cacheRootPath_ = std::move(bundle.cacheRootPath);
    colAndreasPath_ = std::move(bundle.colAndreasPath);
    runtimeNotes_ = std::move(bundle.runtimeNotes);

    if (imgPathEdit_) {
        imgPathEdit_->setText(imgPath_);
    }

    const bool preferTerrain = mapModeCombo_ && mapModeCombo_->currentIndex() == 1 && terrainMapAsset_.has_value();
    rebuildMapModeCombo();
    if (preferTerrain && terrainMapAsset_.has_value()) {
        mapModeCombo_->setCurrentIndex(1);
    }

    mapWidget_->setMapImage(currentMapAsset().image);
    rebuildMapOverlays();
    updateSourceLabel();
}

void MainWindow::rebuildMapModeCombo() {
    if (!mapModeCombo_) {
        return;
    }

    const QSignalBlocker blocker(mapModeCombo_);
    mapModeCombo_->clear();
    mapModeCombo_->addItem(QStringLiteral("Radar Map"));
    if (terrainMapAsset_.has_value()) {
        mapModeCombo_->addItem(QStringLiteral("Terrain Map"));
    }
}

void MainWindow::handleMapClicked(double worldX, double worldY) {
    updateCoordinateLabels(worldX, worldY);

    switch (currentToolMode()) {
    case EditorToolMode::Inspect:
        break;
    case EditorToolMode::Vehicle:
        addVehicleAt(worldX, worldY);
        break;
    case EditorToolMode::PlayerSpawn:
        addPlayerSpawnAt(worldX, worldY);
        break;
    case EditorToolMode::GangZone:
        break;
    }
}

void MainWindow::handleMapHovered(double worldX, double worldY, bool insideMap) {
    if (!insideMap) {
        hoverLabel_->setText(QStringLiteral("Hover: outside map"));
        return;
    }

    const auto z = lookupZ(worldX, worldY);
    const QString zone = lookupZoneName(worldX, worldY, z);
    hoverLabel_->setText(
        QStringLiteral("Hover: X %1    Y %2    Z %3    Zone %4")
            .arg(QString::number(worldX, 'f', 3))
            .arg(QString::number(worldY, 'f', 3))
            .arg(z.has_value() ? QString::number(*z, 'f', 2) : QStringLiteral("n/a"))
            .arg(zone.isEmpty() ? QStringLiteral("(none)") : zone));
}

void MainWindow::handleRectangleSelectionFinished(double minX, double minY, double maxX, double maxY) {
    if (currentToolMode() != EditorToolMode::GangZone) {
        return;
    }
    addGangZone(QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized());
}

void MainWindow::copyCurrentCoordinates() {
    if (currentCoordinateText_.isEmpty()) {
        return;
    }
    QApplication::clipboard()->setText(currentCoordinateText_);
}

void MainWindow::updateZoomLabel(double zoomFactor) {
    zoomLabel_->setText(QStringLiteral("Zoom %1%").arg(QString::number(zoomFactor * 100.0, 'f', 0)));
}

void MainWindow::handleMapModeChanged() {
    mapWidget_->setMapImage(currentMapAsset().image);
    rebuildMapOverlays();
    updateSourceLabel();
}

void MainWindow::updateToolModeUi() {
    const EditorToolMode mode = currentToolMode();
    toolSettingsStack_->setCurrentIndex(toolModeCombo_->currentIndex());

    switch (mode) {
    case EditorToolMode::Inspect:
        toolHintLabel_->setText(QStringLiteral("Move over the map to inspect XYZ and the current SA:MP zone. Left click stores the current coordinate."));
        mapWidget_->setInteractionMode(MapWidget::InteractionMode::PointPick);
        break;
    case EditorToolMode::Vehicle:
        toolHintLabel_->setText(QStringLiteral("Left click places a vehicle spawn using the form values below."));
        mapWidget_->setInteractionMode(MapWidget::InteractionMode::PointPick);
        break;
    case EditorToolMode::PlayerSpawn:
        toolHintLabel_->setText(QStringLiteral("Left click places a player spawn and exports it as AddPlayerClassEx."));
        mapWidget_->setInteractionMode(MapWidget::InteractionMode::PointPick);
        break;
    case EditorToolMode::GangZone:
        toolHintLabel_->setText(QStringLiteral("Drag a rectangle on the map to define a GangZoneCreate area."));
        mapWidget_->setInteractionMode(MapWidget::InteractionMode::RectangleSelect);
        break;
    }
}

void MainWindow::handleItemSelectionChanged() {
    rebuildMapOverlays();

    const EditorItem* item = findItemById(selectedItemId());
    if (!item) {
        return;
    }

    if (item->type == EditorItemType::GangZone) {
        const QPointF center = item->rectWorld.center();
        mapWidget_->setMarkerWorldPosition(center.x(), center.y());
        updateCoordinateLabels(center.x(), center.y());
    } else {
        mapWidget_->setMarkerWorldPosition(item->pointWorld.x(), item->pointWorld.y());
        updateCoordinateLabels(item->pointWorld.x(), item->pointWorld.y());
    }
}

void MainWindow::deleteSelectedItem() {
    const int id = selectedItemId();
    if (id < 0) {
        return;
    }

    items_.erase(std::remove_if(items_.begin(), items_.end(), [id](const EditorItem& item) { return item.id == id; }), items_.end());
    refreshItemsUi();
}

void MainWindow::clearItems() {
    items_.clear();
    refreshItemsUi();
}

void MainWindow::copySelectedItemPawn() {
    const EditorItem* item = findItemById(selectedItemId());
    if (!item) {
        return;
    }

    int ordinal = 1;
    for (const EditorItem& candidate : items_) {
        if (candidate.id == item->id) {
            break;
        }
        ++ordinal;
    }

    QApplication::clipboard()->setText(generatePawnForItem(*item, ordinal));
}

void MainWindow::copyExport() {
    QApplication::clipboard()->setText(exportPreview_->toPlainText());
}

void MainWindow::saveExport() {
    const QString format = exportFormatCombo_->currentData().toString();
    const QString defaultName = format == QStringLiteral("json") ? QStringLiteral("sa-mp-map-studio_export.json") : QStringLiteral("sa-mp-map-studio_export.pwn");
    const QString filter = format == QStringLiteral("json") ? QStringLiteral("JSON Files (*.json)") : QStringLiteral("Pawn Files (*.pwn);;Text Files (*.txt)");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Export"), defaultName, filter);
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(exportPreview_->toPlainText().toUtf8());
}

void MainWindow::updateExportPreview() {
    if (exportFormatCombo_->currentData().toString() == QStringLiteral("json")) {
        exportPreview_->setPlainText(generateJsonExport());
    } else {
        exportPreview_->setPlainText(generatePawnExport());
    }
}

void MainWindow::browseImgPath() {
    const QString path =
        QFileDialog::getOpenFileName(this,
                                     QStringLiteral("Select gta3.img"),
                                     imgPathEdit_->text().trimmed(),
                                     QStringLiteral("GTA IMG Archives (gta3.img);;All Files (*)"));
    if (!path.isEmpty()) {
        imgPathEdit_->setText(path);
    }
}

void MainWindow::reloadRuntimeSources() {
    RuntimeAssetManager::setConfiguredImgPath(imgPathEdit_->text().trimmed());

    RuntimeAssetBundle bundle;
    QString errorMessage;
    if (!RuntimeAssetManager::loadRuntimeBundle(imgPathEdit_->text().trimmed(),
                                                txdPath_,
                                                terrainMapPath_,
                                                QString(),
                                                &bundle,
                                                &errorMessage,
                                                true)) {
        QMessageBox::critical(this, QStringLiteral("SA:MP Map Studio"), errorMessage);
        return;
    }

    applyRuntimeBundle(std::move(bundle));
}

void MainWindow::downloadManagedHeightmap() {
    QString downloadedPath;
    QString errorMessage;
    if (!RuntimeAssetManager::downloadMapAndreasHeightmap(&downloadedPath, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("SA:MP Map Studio"), errorMessage);
        return;
    }

    reloadRuntimeSources();
}

void MainWindow::updateCoordinateLabels(double worldX, double worldY) {
    const auto z = lookupZ(worldX, worldY);
    const QString xText = QString::number(worldX, 'f', 3);
    const QString yText = QString::number(worldY, 'f', 3);
    const QString zText = z.has_value() ? QString::number(*z, 'f', 2) : QStringLiteral("n/a");

    currentCoordinateText_ = QStringLiteral("%1, %2, %3").arg(xText, yText, zText);
    xyzLabel_->setText(QStringLiteral("X: %1    Y: %2    Z: %3").arg(xText, yText, zText));
}

void MainWindow::updateSourceLabel() {
    const QString zonesText = zoneDatabase_.isLoaded()
                                  ? QStringLiteral("Zones: SA:MP zone dataset loaded")
                                  : QStringLiteral("Zones: %1").arg(zoneDatabase_.loadError().isEmpty() ? QStringLiteral("not available") : zoneDatabase_.loadError());
    sourceLabel_->setText(
        QStringLiteral("Map: %1\nGTA3 IMG: %2\nHeightmap: %3\n%4")
            .arg(currentMapAsset().description.isEmpty() ? QStringLiteral("(not found)") : currentMapAsset().description)
            .arg(imgPath_.isEmpty() ? QStringLiteral("(not configured)") : imgPath_)
            .arg(hmapPath_.isEmpty() ? QStringLiteral("(not found)") : hmapPath_)
            .arg(zonesText));

    if (cachePathLabel_) {
        cachePathLabel_->setText(QStringLiteral("Runtime cache: %1").arg(cacheRootPath_.isEmpty() ? QStringLiteral("(not initialized)") : cacheRootPath_));
    }
    if (heightmapStatusLabel_) {
        heightmapStatusLabel_->setText(
            QStringLiteral("MapAndreas: %1")
                .arg(hmapPath_.isEmpty() ? QStringLiteral("not available locally") : hmapPath_));
    }
    if (colAndreasStatusLabel_) {
        const QString colState = QFileInfo::exists(colAndreasPath_)
                                     ? QStringLiteral("%1 (reserved for a future ColAndreas backend)")
                                     : QStringLiteral("%1 (reserved path, not downloaded yet)").arg(colAndreasPath_);
        colAndreasStatusLabel_->setText(QStringLiteral("ColAndreas cache: %1").arg(colState));
    }
    if (runtimeNotesEdit_) {
        runtimeNotesEdit_->setPlainText(runtimeNotes_.trimmed().isEmpty() ? QStringLiteral("No runtime notes.") : runtimeNotes_.trimmed());
    }
}

void MainWindow::addVehicleAt(double worldX, double worldY) {
    EditorItem item;
    item.id = nextItemId_++;
    item.type = EditorItemType::Vehicle;
    item.name = vehicleNameEdit_->text().trimmed().isEmpty() ? defaultItemName(item.type) : vehicleNameEdit_->text().trimmed();
    item.pointWorld = QPointF(worldX, worldY);
    item.z = lookupZ(worldX, worldY).value_or(0.0);
    item.zoneName = lookupZoneName(worldX, worldY, item.z);
    item.angle = vehicleAngleSpin_->value();
    item.modelId = vehicleModelSpin_->value();
    item.color1 = vehicleColor1Spin_->value();
    item.color2 = vehicleColor2Spin_->value();
    item.respawnDelay = vehicleRespawnSpin_->value();
    item.displayColor = QColor(255, 170, 60, 210);
    items_.push_back(std::move(item));
    refreshItemsUi(items_.back().id);
}

void MainWindow::addPlayerSpawnAt(double worldX, double worldY) {
    EditorItem item;
    item.id = nextItemId_++;
    item.type = EditorItemType::PlayerSpawn;
    item.name = spawnNameEdit_->text().trimmed().isEmpty() ? defaultItemName(item.type) : spawnNameEdit_->text().trimmed();
    item.pointWorld = QPointF(worldX, worldY);
    item.z = lookupZ(worldX, worldY).value_or(0.0);
    item.zoneName = lookupZoneName(worldX, worldY, item.z);
    item.angle = spawnAngleSpin_->value();
    item.team = spawnTeamSpin_->value();
    item.skin = spawnSkinSpin_->value();
    item.weapons = {spawnWeapon1Spin_->value(), spawnWeapon2Spin_->value(), spawnWeapon3Spin_->value()};
    item.ammo = {spawnAmmo1Spin_->value(), spawnAmmo2Spin_->value(), spawnAmmo3Spin_->value()};
    item.displayColor = QColor(98, 201, 122, 220);
    items_.push_back(std::move(item));
    refreshItemsUi(items_.back().id);
}

void MainWindow::addGangZone(const QRectF& rectWorld) {
    EditorItem item;
    item.id = nextItemId_++;
    item.type = EditorItemType::GangZone;
    item.name = gangZoneNameEdit_->text().trimmed().isEmpty() ? defaultItemName(item.type) : gangZoneNameEdit_->text().trimmed();
    item.rectWorld = rectWorld.normalized();
    const QPointF center = item.rectWorld.center();
    item.z = lookupZ(center.x(), center.y()).value_or(0.0);
    item.zoneName = lookupZoneName(center.x(), center.y(), item.z);
    item.displayColor = currentGangZoneColor();
    items_.push_back(std::move(item));
    mapWidget_->setMarkerWorldPosition(center.x(), center.y());
    refreshItemsUi(items_.back().id);
}

void MainWindow::rebuildItemTable(int selectedId) {
    QSignalBlocker blocker(itemTable_);
    itemTable_->setRowCount(items_.size());

    for (int row = 0; row < items_.size(); ++row) {
        const EditorItem& item = items_[row];

        auto* typeItem = new QTableWidgetItem(editorItemTypeName(item.type));
        typeItem->setData(Qt::UserRole, item.id);
        auto* nameItem = new QTableWidgetItem(item.name);
        auto* zoneItem = new QTableWidgetItem(item.zoneName.isEmpty() ? QStringLiteral("(none)") : item.zoneName);
        auto* summaryItem = new QTableWidgetItem(itemSummary(item));

        itemTable_->setItem(row, 0, typeItem);
        itemTable_->setItem(row, 1, nameItem);
        itemTable_->setItem(row, 2, zoneItem);
        itemTable_->setItem(row, 3, summaryItem);

        if (item.id == selectedId) {
            itemTable_->selectRow(row);
        }
    }

    if (selectedId < 0) {
        itemTable_->clearSelection();
    }
}

void MainWindow::rebuildMapOverlays() {
    const int selectedId = selectedItemId();
    QVector<MapOverlay> overlays;
    overlays.reserve(items_.size());

    for (const EditorItem& item : items_) {
        MapOverlay overlay;
        overlay.selected = item.id == selectedId;
        overlay.color = item.displayColor;
        overlay.label = item.name;
        if (item.type == EditorItemType::GangZone) {
            overlay.kind = MapOverlay::Kind::Rectangle;
            overlay.rectWorld = item.rectWorld;
        } else {
            overlay.kind = MapOverlay::Kind::Point;
            overlay.pointWorld = item.pointWorld;
        }
        overlays.push_back(std::move(overlay));
    }

    mapWidget_->setOverlays(overlays);
}

void MainWindow::refreshItemsUi(int selectedId) {
    rebuildItemTable(selectedId);
    rebuildMapOverlays();
    updateExportPreview();
}
