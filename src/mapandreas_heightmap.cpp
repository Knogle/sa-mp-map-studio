#include "mapandreas_heightmap.h"

#include <QFile>

namespace {
constexpr qint64 kExpectedBytes =
    static_cast<qint64>(MapAndreasHeightMap::kPointCount) * sizeof(unsigned short);
}

bool MapAndreasHeightMap::load(const QString& path, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open HMAP file: %1").arg(path);
        }
        return false;
    }

    if (file.size() != kExpectedBytes) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unexpected HMAP size for %1: got %2 bytes, expected %3")
                                .arg(path)
                                .arg(file.size())
                                .arg(kExpectedBytes);
        }
        return false;
    }

    points_.resize(kPointCount);
    const auto bytesRead =
        file.read(reinterpret_cast<char*>(points_.data()), static_cast<qint64>(points_.size() * sizeof(unsigned short)));
    if (bytesRead != kExpectedBytes) {
        points_.clear();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not read full HMAP payload from %1").arg(path);
        }
        return false;
    }

    sourcePath_ = path;
    return true;
}

bool MapAndreasHeightMap::isLoaded() const noexcept {
    return points_.size() == kPointCount;
}

std::optional<double> MapAndreasHeightMap::lookupZ(double x, double y) const {
    if (!isLoaded()) {
        return std::nullopt;
    }

    // Match MapAndreas' original truncation and grid mapping.
    if (x < -3000.0 || x > 3000.0 || y > 3000.0 || y < -3000.0) {
        return std::nullopt;
    }

    const int gridX = static_cast<int>(x) + 3000;
    const int gridY = (static_cast<int>(y) - 3000) * -1;

    if (gridX < 0 || gridX >= kWidth || gridY < 0 || gridY >= kHeight) {
        return std::nullopt;
    }

    const std::size_t index = static_cast<std::size_t>(gridY) * kWidth + static_cast<std::size_t>(gridX);
    return static_cast<double>(points_[index]) / 100.0;
}

const QString& MapAndreasHeightMap::sourcePath() const noexcept {
    return sourcePath_;
}
