#pragma once

#include <optional>
#include <QString>
#include <vector>

class MapAndreasHeightMap {
public:
    static constexpr int kWidth = 6000;
    static constexpr int kHeight = 6000;
    static constexpr int kPointCount = kWidth * kHeight;

    bool load(const QString& path, QString* errorMessage = nullptr);

    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] std::optional<double> lookupZ(double x, double y) const;
    [[nodiscard]] const QString& sourcePath() const noexcept;

private:
    QString sourcePath_;
    std::vector<unsigned short> points_;
};
