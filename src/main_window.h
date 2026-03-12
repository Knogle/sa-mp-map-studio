#pragma once

#include "editor_types.h"
#include "mapandreas_heightmap.h"
#include "sa_map_loader.h"
#include "samp_zone_database.h"

#include <QLabel>
#include <QMainWindow>
#include <QString>
#include <QVector>

#include <optional>

class MapWidget;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(SaMapAsset radarMapAsset,
               std::optional<SaMapAsset> terrainMapAsset,
               MapAndreasHeightMap heightMap,
               QString hmapPath,
               QWidget* parent = nullptr);

private slots:
    void handleMapClicked(double worldX, double worldY);
    void handleMapHovered(double worldX, double worldY, bool insideMap);
    void handleRectangleSelectionFinished(double minX, double minY, double maxX, double maxY);
    void copyCurrentCoordinates();
    void updateZoomLabel(double zoomFactor);
    void handleMapModeChanged();
    void updateToolModeUi();
    void handleItemSelectionChanged();
    void deleteSelectedItem();
    void clearItems();
    void copySelectedItemPawn();
    void copyExport();
    void saveExport();
    void updateExportPreview();

private:
    [[nodiscard]] EditorToolMode currentToolMode() const;
    [[nodiscard]] std::optional<double> lookupZ(double worldX, double worldY) const;
    [[nodiscard]] QString lookupZoneName(double worldX, double worldY, std::optional<double> z) const;
    [[nodiscard]] QString defaultItemName(EditorItemType type) const;
    [[nodiscard]] int selectedItemId() const;
    [[nodiscard]] const EditorItem* findItemById(int id) const;
    [[nodiscard]] QColor currentGangZoneColor() const;
    [[nodiscard]] QString itemSummary(const EditorItem& item) const;
    [[nodiscard]] QString generatePawnForItem(const EditorItem& item, int ordinal) const;
    [[nodiscard]] QString generatePawnExport() const;
    [[nodiscard]] QString generateJsonExport() const;
    [[nodiscard]] const SaMapAsset& currentMapAsset() const;

    void updateCoordinateLabels(double worldX, double worldY);
    void updateSourceLabel();
    void addVehicleAt(double worldX, double worldY);
    void addPlayerSpawnAt(double worldX, double worldY);
    void addGangZone(const QRectF& rectWorld);
    void rebuildItemTable(int selectedId = -1);
    void rebuildMapOverlays();
    void refreshItemsUi(int selectedId = -1);

    MapWidget* mapWidget_ = nullptr;
    QLabel* xyzLabel_ = nullptr;
    QLabel* hoverLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QComboBox* mapModeCombo_ = nullptr;
    QPushButton* copyButton_ = nullptr;
    QPushButton* zoomInButton_ = nullptr;
    QPushButton* zoomOutButton_ = nullptr;
    QPushButton* resetZoomButton_ = nullptr;

    QComboBox* toolModeCombo_ = nullptr;
    QLabel* toolHintLabel_ = nullptr;
    QStackedWidget* toolSettingsStack_ = nullptr;

    QLineEdit* vehicleNameEdit_ = nullptr;
    QSpinBox* vehicleModelSpin_ = nullptr;
    QDoubleSpinBox* vehicleAngleSpin_ = nullptr;
    QSpinBox* vehicleColor1Spin_ = nullptr;
    QSpinBox* vehicleColor2Spin_ = nullptr;
    QSpinBox* vehicleRespawnSpin_ = nullptr;

    QLineEdit* spawnNameEdit_ = nullptr;
    QSpinBox* spawnSkinSpin_ = nullptr;
    QSpinBox* spawnTeamSpin_ = nullptr;
    QDoubleSpinBox* spawnAngleSpin_ = nullptr;
    QSpinBox* spawnWeapon1Spin_ = nullptr;
    QSpinBox* spawnAmmo1Spin_ = nullptr;
    QSpinBox* spawnWeapon2Spin_ = nullptr;
    QSpinBox* spawnAmmo2Spin_ = nullptr;
    QSpinBox* spawnWeapon3Spin_ = nullptr;
    QSpinBox* spawnAmmo3Spin_ = nullptr;

    QLineEdit* gangZoneNameEdit_ = nullptr;
    QLineEdit* gangZoneColorEdit_ = nullptr;

    QTableWidget* itemTable_ = nullptr;
    QPushButton* removeItemButton_ = nullptr;
    QPushButton* clearItemsButton_ = nullptr;
    QPushButton* copySelectedPawnButton_ = nullptr;

    QComboBox* exportFormatCombo_ = nullptr;
    QPushButton* copyExportButton_ = nullptr;
    QPushButton* saveExportButton_ = nullptr;
    QPlainTextEdit* exportPreview_ = nullptr;

    MapAndreasHeightMap heightMap_;
    SampZoneDatabase zoneDatabase_;
    SaMapAsset radarMapAsset_;
    std::optional<SaMapAsset> terrainMapAsset_;
    QString hmapPath_;
    QString currentCoordinateText_;
    QVector<EditorItem> items_;
    int nextItemId_ = 1;
};
