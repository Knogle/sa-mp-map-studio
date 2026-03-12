#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <optional>

class SampZoneDatabase {
public:
    SampZoneDatabase();

    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] const QString& loadError() const noexcept;
    [[nodiscard]] QString lookup(double x, double y, std::optional<double> z = std::nullopt) const;

private:
    enum class ShapeType {
        Cuboid,
        Polygon,
    };

    struct Zone {
        QString name;
        ShapeType shape = ShapeType::Cuboid;
        QRectF bounds2D;
        double minZ = 0.0;
        double maxZ = 0.0;
        QVector<QPointF> points;
    };

    [[nodiscard]] static bool pointInPolygon(const QVector<QPointF>& polygon, const QPointF& point);
    [[nodiscard]] bool contains(const Zone& zone, double x, double y, std::optional<double> z) const;

    QVector<Zone> zones_;
    QString loadError_;
};
