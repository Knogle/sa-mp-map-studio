#pragma once

#include "editor_types.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QWidget>

class MapWidget : public QWidget {
    Q_OBJECT

public:
    enum class InteractionMode {
        PointPick,
        RectangleSelect,
    };

    explicit MapWidget(QWidget* parent = nullptr);

    void setMapImage(const QImage& image);
    void setMarkerWorldPosition(double worldX, double worldY);
    void setInteractionMode(InteractionMode mode);
    void setOverlays(const QVector<MapOverlay>& overlays);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    [[nodiscard]] double zoomFactor() const;

signals:
    void mapClicked(double worldX, double worldY);
    void mapHovered(double worldX, double worldY, bool insideMap);
    void rectangleSelectionFinished(double minX, double minY, double maxX, double maxY);
    void zoomChanged(double zoomFactor);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    [[nodiscard]] QRectF imageRect() const;
    [[nodiscard]] QRectF sourceImageRect() const;
    [[nodiscard]] bool widgetPointToWorld(const QPointF& point, double* outWorldX, double* outWorldY) const;
    [[nodiscard]] QPointF worldToWidgetPoint(double worldX, double worldY) const;
    void emitHoverAt(const QPointF& widgetPoint);
    void setZoomFactor(double zoomFactor, const QPointF* anchorWidgetPoint = nullptr);
    void panByWidgetDelta(const QPointF& delta);
    void clampViewCenter();
    [[nodiscard]] double visibleWorldSpan() const;

    QImage mapImage_;
    QVector<MapOverlay> overlays_;
    InteractionMode interactionMode_ = InteractionMode::PointPick;

    bool hasMarker_ = false;
    double markerWorldX_ = 0.0;
    double markerWorldY_ = 0.0;

    double zoomFactor_ = 1.0;
    double viewCenterWorldX_ = 0.0;
    double viewCenterWorldY_ = 0.0;

    bool panning_ = false;
    QPointF lastPanPosition_;

    bool selectionActive_ = false;
    QPointF selectionStartWorld_;
    QPointF selectionCurrentWorld_;
};
