#ifndef DIRECTIONALPAD_H
#define DIRECTIONALPAD_H

#include <QWidget>
#include <QPainterPath>
enum class DirectionRegion {
    None,
    Center,
    Up,
    Down,
    Left,
    Right
};
class DirectionalPad : public QWidget
{
    Q_OBJECT
public:
    explicit DirectionalPad(QWidget *parent = nullptr);
    ~DirectionalPad() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updatePaths();
    DirectionRegion hitTest(const QPointF &pos) const;
    QString regionName(DirectionRegion region) const;

    DirectionRegion m_hoveredRegion;
    DirectionRegion m_pressedRegion;

    QPainterPath m_centerPath;
    QPainterPath m_upPath;
    QPainterPath m_downPath;
    QPainterPath m_leftPath;
    QPainterPath m_rightPath;
signals:
    void press(DirectionRegion r);
    void release(DirectionRegion r);
};

#endif // DIRECTIONALPAD_H
