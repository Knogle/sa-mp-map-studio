#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <array>

enum class EditorToolMode {
    Inspect,
    Vehicle,
    PlayerSpawn,
    GangZone,
};

enum class EditorItemType {
    Vehicle,
    PlayerSpawn,
    GangZone,
};

struct MapOverlay {
    enum class Kind {
        Point,
        Rectangle,
    };

    Kind kind = Kind::Point;
    QPointF pointWorld;
    QRectF rectWorld;
    QColor color;
    QString label;
    bool selected = false;
};

struct EditorItem {
    int id = 0;
    EditorItemType type = EditorItemType::Vehicle;
    QString name;
    QString zoneName;

    QPointF pointWorld;
    QRectF rectWorld;
    double z = 0.0;
    double angle = 0.0;

    int modelId = 411;
    int color1 = -1;
    int color2 = -1;
    int respawnDelay = 120;

    int team = 0;
    int skin = 0;
    std::array<int, 3> weapons = {0, 0, 0};
    std::array<int, 3> ammo = {0, 0, 0};

    QColor displayColor;
};

inline QString editorItemTypeName(EditorItemType type) {
    switch (type) {
    case EditorItemType::Vehicle:
        return QStringLiteral("Vehicle");
    case EditorItemType::PlayerSpawn:
        return QStringLiteral("Spawn");
    case EditorItemType::GangZone:
        return QStringLiteral("Gang Zone");
    }

    return QStringLiteral("Item");
}
