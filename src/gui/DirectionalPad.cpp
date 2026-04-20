#include "DirectionalPad.h"

#include <QPainter>
#include <QMouseEvent>
#include <QStyleOption>
#include <QToolTip>
#include <QDebug>
#include <qmath.h>

DirectionalPad::DirectionalPad(QWidget *parent)
    : QWidget(parent),
      m_hoveredRegion(DirectionRegion::None),
      m_pressedRegion(DirectionRegion::None)
{
    setMouseTracking(true); // Required for hover effects
    setMinimumSize(100, 100);
}

void DirectionalPad::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePaths();
}

void DirectionalPad::updatePaths()
{
    QRectF rect(0, 0, width(), height());
    qreal size = qMin(rect.width(), rect.height());
    // Margin for the widget
    qreal margin = 5.0;
    size -= margin * 2;
    
    QRectF outerRect(
        rect.center().x() - size / 2,
        rect.center().y() - size / 2,
        size, size
    );

    qreal innerSize = size * 0.4;
    QRectF innerRect(
        outerRect.center().x() - innerSize / 2,
        outerRect.center().y() - innerSize / 2,
        innerSize, innerSize
    );

    m_centerPath = QPainterPath();
    m_centerPath.addEllipse(innerRect);

    auto makeOuterPath = [&](qreal startAngle) {
        QPainterPath pie;
        pie.moveTo(outerRect.center());
        // arcTo angles in Qt: 0 is 3 o'clock, positive is counter-clockwise.
        // We want 90 degrees span.
        // Right: -45 to 45
        // Up: 45 to 135
        // Left: 135 to 225
        // Down: 225 to 315
        pie.arcTo(outerRect, startAngle, 90);
        pie.closeSubpath();
        
        // Subtract the center path to create a donut slice
        return pie.subtracted(m_centerPath);
    };

    m_rightPath = makeOuterPath(-45.0);
    m_upPath    = makeOuterPath(45.0);
    m_leftPath  = makeOuterPath(135.0);
    m_downPath  = makeOuterPath(225.0);
}

DirectionRegion DirectionalPad::hitTest(const QPointF &pos) const
{
    if (m_centerPath.contains(pos)) return DirectionRegion::Center;
    if (m_upPath.contains(pos))     return DirectionRegion::Up;
    if (m_downPath.contains(pos))   return DirectionRegion::Down;
    if (m_leftPath.contains(pos))   return DirectionRegion::Left;
    if (m_rightPath.contains(pos))  return DirectionRegion::Right;
    return DirectionRegion::None;
}

QString DirectionalPad::regionName(DirectionRegion region) const
{
    switch (region) {
        case DirectionRegion::Center: return "复位 (Reset)";
        case DirectionRegion::Up:     return "向上 (Up)";
        case DirectionRegion::Down:   return "向下 (Down)";
        case DirectionRegion::Left:   return "向左 (Left)";
        case DirectionRegion::Right:  return "向右 (Right)";
        default:     return "";
    }
}

void DirectionalPad::mouseMoveEvent(QMouseEvent *event)
{
    DirectionRegion newHover = hitTest(event->position());
    if (m_hoveredRegion != newHover) {
        m_hoveredRegion = newHover;
        update();
        
        if (m_hoveredRegion != DirectionRegion::None) {
            QToolTip::showText(event->globalPosition().toPoint(), regionName(m_hoveredRegion), this);
        } else {
            QToolTip::hideText();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void DirectionalPad::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressedRegion = hitTest(event->position());
        if (m_pressedRegion != DirectionRegion::None) {
            // qDebug() << "[ControlPanel] 按下:" << regionName(m_pressedRegion);
            emit press(m_pressedRegion);
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void DirectionalPad::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        DirectionRegion releaseRegion = hitTest(event->position());
        if (m_pressedRegion != DirectionRegion::None) {
            if (m_pressedRegion == releaseRegion) {
                // This counts as a valid click
                // qDebug() << "[ControlPanel] 松开(触发):" << regionName(m_pressedRegion);
                emit release(releaseRegion);
            }
        }
        m_pressedRegion = DirectionRegion::None;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void DirectionalPad::leaveEvent(QEvent *event)
{
    if (m_hoveredRegion != DirectionRegion::None) {
        m_hoveredRegion = DirectionRegion::None;
        update();
    }
    QWidget::leaveEvent(event);
}

void DirectionalPad::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPalette pal = palette();

    // Helper to draw a single region
    auto drawRegion = [&](const QPainterPath &path, DirectionRegion region) {
        QColor fillColor = pal.button().color();
        
        if (m_pressedRegion == region) {
            fillColor = pal.mid().color(); // Pressed state
        } else if (m_hoveredRegion == region) {
            fillColor = pal.light().color(); // Hover state
        }

        // Draw background gap matching window color
        painter.setPen(QPen(pal.window().color(), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(fillColor);
        painter.drawPath(path);
    };

    drawRegion(m_centerPath, DirectionRegion::Center);
    drawRegion(m_upPath, DirectionRegion::Up);
    drawRegion(m_downPath, DirectionRegion::Down);
    drawRegion(m_leftPath, DirectionRegion::Left);
    drawRegion(m_rightPath, DirectionRegion::Right);

    // Draw Icons
    painter.setPen(QPen(pal.buttonText().color(), 2.0));
    painter.setBrush(pal.buttonText().color());

    // 1. Draw Outer Direction Arrows (triangles)
    auto drawArrow = [&](qreal angle, const QRectF &outerRect) {
        painter.save();
        painter.translate(outerRect.center());
        painter.rotate(-angle); // Rotate to the angle (negative because Qt rotate is clockwise, our angle is CCW)
        // Move to the middle of the outer pie (radius is 0.7 * size/2 roughly)
        qreal r = outerRect.width() / 2.0 * 0.75;
        painter.translate(r, 0);
        painter.rotate(90); // Point outward
        
        // Draw small triangle
        QPolygonF poly;
        poly << QPointF(0, -6) << QPointF(-5, 4) << QPointF(5, 4);
        painter.drawPolygon(poly);
        painter.restore();
    };

    QRectF rect(0, 0, width(), height());
    qreal size = qMin(rect.width(), rect.height()) - 10.0;
    QRectF outerRect(rect.center().x() - size / 2, rect.center().y() - size / 2, size, size);

    drawArrow(0, outerRect);   // Right
    drawArrow(90, outerRect);  // Up
    drawArrow(180, outerRect); // Left
    drawArrow(270, outerRect); // Down

    // 2. Draw Center Reset Icon (circular arrow)
    painter.save();
    painter.translate(outerRect.center());
    painter.setPen(QPen(pal.buttonText().color(), 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    
    qreal resetRadius = size * 0.12;
    // Draw an arc from 45 to 315 degrees
    painter.drawArc(QRectF(-resetRadius, -resetRadius, resetRadius*2, resetRadius*2), 45 * 16, 270 * 16);
    
    // Draw the arrowhead at the end of the arc (near 45 degrees)
    painter.setBrush(pal.buttonText().color());
    painter.setPen(Qt::NoPen);
    painter.rotate(-45);
    painter.translate(resetRadius, 0);
    
    QPolygonF head;
    head << QPointF(0, 4) << QPointF(-4, -2) << QPointF(4, -2);
    painter.drawPolygon(head);
    painter.restore();
}
