#include "samp_zone_database.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr const char* kZoneResourcePath = ":/data/samp_zones.json";

QRectF normalizedRect(double minX, double minY, double maxX, double maxY) {
    return QRectF(QPointF(std::min(minX, maxX), std::min(minY, maxY)),
                  QPointF(std::max(minX, maxX), std::max(minY, maxY)));
}

} // namespace

SampZoneDatabase::SampZoneDatabase() {
    QFile file(QString::fromUtf8(kZoneResourcePath));
    if (!file.open(QIODevice::ReadOnly)) {
        loadError_ = QStringLiteral("Could not open zone data resource %1").arg(QString::fromUtf8(kZoneResourcePath));
        return;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        loadError_ = QStringLiteral("Zone data resource %1 is not a JSON array").arg(QString::fromUtf8(kZoneResourcePath));
        return;
    }

    const QJsonArray zones = document.array();
    zones_.reserve(zones.size());

    for (const QJsonValue& value : zones) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        Zone zone;
        zone.name = object.value(QStringLiteral("name")).toString();
        const QString shapeName = object.value(QStringLiteral("shape")).toString();
        zone.shape = shapeName == QStringLiteral("polygon") ? ShapeType::Polygon : ShapeType::Cuboid;
        zone.bounds2D = normalizedRect(object.value(QStringLiteral("minX")).toDouble(),
                                       object.value(QStringLiteral("minY")).toDouble(),
                                       object.value(QStringLiteral("maxX")).toDouble(),
                                       object.value(QStringLiteral("maxY")).toDouble());
        zone.minZ = object.contains(QStringLiteral("minZ"))
                        ? object.value(QStringLiteral("minZ")).toDouble()
                        : -std::numeric_limits<double>::infinity();
        zone.maxZ = object.value(QStringLiteral("maxZ")).toDouble(std::numeric_limits<double>::infinity());

        if (zone.shape == ShapeType::Polygon) {
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            zone.points.reserve(points.size());
            for (const QJsonValue& pointValue : points) {
                const QJsonArray point = pointValue.toArray();
                if (point.size() != 2) {
                    continue;
                }
                zone.points.push_back(QPointF(point.at(0).toDouble(), point.at(1).toDouble()));
            }
        }

        if (!zone.name.isEmpty()) {
            zones_.push_back(std::move(zone));
        }
    }

    if (zones_.isEmpty()) {
        loadError_ = QStringLiteral("Zone data resource %1 did not contain any usable zones").arg(QString::fromUtf8(kZoneResourcePath));
    }
}

bool SampZoneDatabase::isLoaded() const noexcept {
    return !zones_.isEmpty();
}

const QString& SampZoneDatabase::loadError() const noexcept {
    return loadError_;
}

QString SampZoneDatabase::lookup(double x, double y, std::optional<double> z) const {
    for (const Zone& zone : zones_) {
        if (contains(zone, x, y, z)) {
            return zone.name;
        }
    }
    return {};
}

bool SampZoneDatabase::pointInPolygon(const QVector<QPointF>& polygon, const QPointF& point) {
    if (polygon.size() < 3) {
        return false;
    }

    bool inside = false;
    for (int i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const QPointF& a = polygon[i];
        const QPointF& b = polygon[j];
        const bool intersects = ((a.y() > point.y()) != (b.y() > point.y())) &&
                                (point.x() < (b.x() - a.x()) * (point.y() - a.y()) / ((b.y() - a.y()) + 1e-12) + a.x());
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool SampZoneDatabase::contains(const Zone& zone, double x, double y, std::optional<double> z) const {
    if (!zone.bounds2D.contains(QPointF(x, y))) {
        return false;
    }

    if (z.has_value()) {
        if (*z < zone.minZ || *z > zone.maxZ) {
            return false;
        }
    }

    if (zone.shape == ShapeType::Cuboid) {
        return true;
    }

    return pointInPolygon(zone.points, QPointF(x, y));
}
