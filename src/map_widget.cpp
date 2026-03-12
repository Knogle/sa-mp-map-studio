#include "map_widget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kWorldMin = -3000.0;
constexpr double kWorldMax = 3000.0;
constexpr double kWorldSpan = kWorldMax - kWorldMin;
constexpr double kMinZoomFactor = 1.0;
constexpr double kMaxZoomFactor = 32.0;
constexpr double kWheelZoomStep = 1.2;

QRectF normalizedRectFromPoints(const QPointF& a, const QPointF& b) {
    return QRectF(QPointF(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                  QPointF(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

} // namespace

MapWidget::MapWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(512, 512);
    setMouseTracking(true);
}

void MapWidget::setMapImage(const QImage& image) {
    mapImage_ = image;
    zoomFactor_ = 1.0;
    viewCenterWorldX_ = 0.0;
    viewCenterWorldY_ = 0.0;
    panning_ = false;
    selectionActive_ = false;
    emit zoomChanged(zoomFactor_);
    update();
}

void MapWidget::setMarkerWorldPosition(double worldX, double worldY) {
    hasMarker_ = true;
    markerWorldX_ = worldX;
    markerWorldY_ = worldY;
    update();
}

void MapWidget::setInteractionMode(InteractionMode mode) {
    interactionMode_ = mode;
    selectionActive_ = false;
    update();
}

void MapWidget::setOverlays(const QVector<MapOverlay>& overlays) {
    overlays_ = overlays;
    update();
}

void MapWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(19, 21, 24));

    if (mapImage_.isNull()) {
        painter.setPen(QColor(210, 210, 210));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("No map image loaded"));
        return;
    }

    const QRectF target = imageRect();
    const QRectF source = sourceImageRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(target, mapImage_, source);

    painter.setPen(QPen(QColor(250, 250, 250, 160), 1.0));
    painter.drawRect(target);

    painter.setClipRect(target.adjusted(1.0, 1.0, -1.0, -1.0));
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const MapOverlay& overlay : overlays_) {
        QColor fillColor = overlay.color.isValid() ? overlay.color : QColor(255, 160, 64, 180);
        QColor strokeColor = fillColor.lighter(overlay.selected ? 160 : 120);
        const double penWidth = overlay.selected ? 3.0 : 2.0;
        painter.setPen(QPen(strokeColor, penWidth));

        if (overlay.kind == MapOverlay::Kind::Point) {
            const QPointF center = worldToWidgetPoint(overlay.pointWorld.x(), overlay.pointWorld.y());
            painter.setBrush(fillColor);
            painter.drawEllipse(center, overlay.selected ? 7.0 : 5.0, overlay.selected ? 7.0 : 5.0);
            painter.drawLine(QPointF(center.x() - 10.0, center.y()), QPointF(center.x() + 10.0, center.y()));
            painter.drawLine(QPointF(center.x(), center.y() - 10.0), QPointF(center.x(), center.y() + 10.0));
            if (!overlay.label.isEmpty()) {
                painter.setPen(QColor(255, 255, 255));
                painter.drawText(center + QPointF(10.0, -10.0), overlay.label);
            }
        } else {
            const QRectF rect = normalizedRectFromPoints(worldToWidgetPoint(overlay.rectWorld.left(), overlay.rectWorld.top()),
                                                         worldToWidgetPoint(overlay.rectWorld.right(), overlay.rectWorld.bottom()));
            QColor shaded = fillColor;
            shaded.setAlpha(overlay.selected ? 90 : 55);
            painter.fillRect(rect, shaded);
            painter.drawRect(rect);
            if (!overlay.label.isEmpty()) {
                painter.setPen(QColor(255, 255, 255));
                painter.drawText(rect.topLeft() + QPointF(6.0, 18.0), overlay.label);
            }
        }
    }

    if (selectionActive_) {
        const QRectF selectionRect = normalizedRectFromPoints(worldToWidgetPoint(selectionStartWorld_.x(), selectionStartWorld_.y()),
                                                              worldToWidgetPoint(selectionCurrentWorld_.x(), selectionCurrentWorld_.y()));
        QColor fill(77, 166, 255, 64);
        painter.fillRect(selectionRect, fill);
        painter.setPen(QPen(QColor(77, 166, 255), 2.0, Qt::DashLine));
        painter.drawRect(selectionRect);
    }

    if (hasMarker_) {
        const QPointF center = worldToWidgetPoint(markerWorldX_, markerWorldY_);
        painter.setPen(QPen(QColor(255, 68, 68), 2.0));
        painter.setBrush(QColor(255, 68, 68));
        painter.drawLine(QPointF(center.x() - 10.0, center.y()), QPointF(center.x() + 10.0, center.y()));
        painter.drawLine(QPointF(center.x(), center.y() - 10.0), QPointF(center.x(), center.y() + 10.0));
        painter.drawEllipse(center, 4.0, 4.0);
    }
}

void MapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        if (imageRect().contains(event->position())) {
            panning_ = true;
            lastPanPosition_ = event->position();
            setCursor(Qt::ClosedHandCursor);
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    double worldX = 0.0;
    double worldY = 0.0;
    if (!widgetPointToWorld(event->position(), &worldX, &worldY)) {
        return;
    }

    if (interactionMode_ == InteractionMode::RectangleSelect) {
        selectionActive_ = true;
        selectionStartWorld_ = QPointF(worldX, worldY);
        selectionCurrentWorld_ = selectionStartWorld_;
        update();
        return;
    }

    setMarkerWorldPosition(worldX, worldY);
    emit mapClicked(worldX, worldY);
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton && panning_) {
        panning_ = false;
        unsetCursor();
        return;
    }

    if (event->button() == Qt::LeftButton && selectionActive_) {
        selectionActive_ = false;
        const QRectF rect = normalizedRectFromPoints(selectionStartWorld_, selectionCurrentWorld_);
        update();
        if (rect.width() >= 1.0 && rect.height() >= 1.0) {
            emit rectangleSelectionFinished(rect.left(), rect.top(), rect.right(), rect.bottom());
        }
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void MapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning_ && (event->buttons() & Qt::RightButton)) {
        const QPointF delta = event->position() - lastPanPosition_;
        lastPanPosition_ = event->position();
        panByWidgetDelta(delta);
    }

    if (selectionActive_ && (event->buttons() & Qt::LeftButton)) {
        double worldX = 0.0;
        double worldY = 0.0;
        if (widgetPointToWorld(event->position(), &worldX, &worldY)) {
            selectionCurrentWorld_ = QPointF(worldX, worldY);
            update();
        }
    }

    emitHoverAt(event->position());
}

void MapWidget::wheelEvent(QWheelEvent* event) {
    if (mapImage_.isNull()) {
        event->ignore();
        return;
    }

    const QPoint angleDelta = event->angleDelta();
    if (angleDelta.y() == 0) {
        event->ignore();
        return;
    }

    const double steps = static_cast<double>(angleDelta.y()) / 120.0;
    const double factor = std::pow(kWheelZoomStep, steps);
    const QPointF cursorPosition = event->position();
    setZoomFactor(zoomFactor_ * factor, &cursorPosition);
    emitHoverAt(event->position());
    event->accept();
}

void MapWidget::leaveEvent(QEvent* event) {
    emit mapHovered(0.0, 0.0, false);
    QWidget::leaveEvent(event);
}

QRectF MapWidget::imageRect() const {
    if (mapImage_.isNull()) {
        return {};
    }

    const QSizeF available = size();
    const QSizeF imageSize = mapImage_.size();
    const double scale = std::min(available.width() / imageSize.width(), available.height() / imageSize.height());
    const QSizeF scaled(imageSize.width() * scale, imageSize.height() * scale);
    const QPointF topLeft((available.width() - scaled.width()) / 2.0, (available.height() - scaled.height()) / 2.0);
    return QRectF(topLeft, scaled);
}

QRectF MapWidget::sourceImageRect() const {
    if (mapImage_.isNull()) {
        return {};
    }

    const double halfSpan = visibleWorldSpan() / 2.0;
    const double worldMinX = viewCenterWorldX_ - halfSpan;
    const double worldMaxX = viewCenterWorldX_ + halfSpan;
    const double worldMaxY = viewCenterWorldY_ + halfSpan;
    const double worldMinY = viewCenterWorldY_ - halfSpan;

    const double left = ((worldMinX - kWorldMin) / kWorldSpan) * mapImage_.width();
    const double right = ((worldMaxX - kWorldMin) / kWorldSpan) * mapImage_.width();
    const double top = ((kWorldMax - worldMaxY) / kWorldSpan) * mapImage_.height();
    const double bottom = ((kWorldMax - worldMinY) / kWorldSpan) * mapImage_.height();
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

bool MapWidget::widgetPointToWorld(const QPointF& point, double* outWorldX, double* outWorldY) const {
    if (mapImage_.isNull()) {
        return false;
    }

    const QRectF drawRect = imageRect();
    if (!drawRect.contains(point)) {
        return false;
    }

    const double relX = (point.x() - drawRect.left()) / drawRect.width();
    const double relY = (point.y() - drawRect.top()) / drawRect.height();

    const double clampedRelX = std::clamp(relX, 0.0, std::nextafter(1.0, 0.0));
    const double clampedRelY = std::clamp(relY, 0.0, std::nextafter(1.0, 0.0));

    const double halfSpan = visibleWorldSpan() / 2.0;
    const double worldMinX = viewCenterWorldX_ - halfSpan;
    const double worldMaxY = viewCenterWorldY_ + halfSpan;

    *outWorldX = worldMinX + clampedRelX * visibleWorldSpan();
    *outWorldY = worldMaxY - clampedRelY * visibleWorldSpan();
    return true;
}

QPointF MapWidget::worldToWidgetPoint(double worldX, double worldY) const {
    const QRectF drawRect = imageRect();
    const double halfSpan = visibleWorldSpan() / 2.0;
    const double worldMinX = viewCenterWorldX_ - halfSpan;
    const double worldMaxY = viewCenterWorldY_ + halfSpan;
    const double relX = (worldX - worldMinX) / visibleWorldSpan();
    const double relY = (worldMaxY - worldY) / visibleWorldSpan();
    return QPointF(drawRect.left() + relX * drawRect.width(), drawRect.top() + relY * drawRect.height());
}

void MapWidget::emitHoverAt(const QPointF& widgetPoint) {
    double worldX = 0.0;
    double worldY = 0.0;
    if (!widgetPointToWorld(widgetPoint, &worldX, &worldY)) {
        emit mapHovered(0.0, 0.0, false);
        return;
    }
    emit mapHovered(worldX, worldY, true);
}

void MapWidget::zoomIn() {
    setZoomFactor(zoomFactor_ * kWheelZoomStep);
}

void MapWidget::zoomOut() {
    setZoomFactor(zoomFactor_ / kWheelZoomStep);
}

void MapWidget::resetZoom() {
    setZoomFactor(1.0);
}

double MapWidget::zoomFactor() const {
    return zoomFactor_;
}

void MapWidget::setZoomFactor(double zoomFactor, const QPointF* anchorWidgetPoint) {
    if (mapImage_.isNull()) {
        return;
    }

    const double clampedZoom = std::clamp(zoomFactor, kMinZoomFactor, kMaxZoomFactor);
    if (std::abs(clampedZoom - zoomFactor_) < 0.0001) {
        return;
    }

    double anchoredWorldX = 0.0;
    double anchoredWorldY = 0.0;
    const QRectF drawRect = imageRect();
    const bool hasAnchor = anchorWidgetPoint && drawRect.contains(*anchorWidgetPoint) &&
                           widgetPointToWorld(*anchorWidgetPoint, &anchoredWorldX, &anchoredWorldY);

    zoomFactor_ = clampedZoom;

    if (hasAnchor) {
        const double relX = std::clamp((anchorWidgetPoint->x() - drawRect.left()) / drawRect.width(), 0.0, 1.0);
        const double relY = std::clamp((anchorWidgetPoint->y() - drawRect.top()) / drawRect.height(), 0.0, 1.0);
        const double span = visibleWorldSpan();
        viewCenterWorldX_ = anchoredWorldX + (0.5 - relX) * span;
        viewCenterWorldY_ = anchoredWorldY + (relY - 0.5) * span;
    }

    clampViewCenter();
    emit zoomChanged(zoomFactor_);
    update();
}

void MapWidget::panByWidgetDelta(const QPointF& delta) {
    const QRectF drawRect = imageRect();
    if (drawRect.isEmpty()) {
        return;
    }

    const double span = visibleWorldSpan();
    viewCenterWorldX_ -= (delta.x() / drawRect.width()) * span;
    viewCenterWorldY_ += (delta.y() / drawRect.height()) * span;
    clampViewCenter();
    update();
}

void MapWidget::clampViewCenter() {
    const double halfSpan = visibleWorldSpan() / 2.0;
    const double minCenter = kWorldMin + halfSpan;
    const double maxCenter = kWorldMax - halfSpan;

    viewCenterWorldX_ = std::clamp(viewCenterWorldX_, minCenter, maxCenter);
    viewCenterWorldY_ = std::clamp(viewCenterWorldY_, minCenter, maxCenter);
}

double MapWidget::visibleWorldSpan() const {
    return kWorldSpan / zoomFactor_;
}
