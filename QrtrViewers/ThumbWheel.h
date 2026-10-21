// ThumbWheel.h
//
// A small Qt widget that imitates the thumbwheels found on the decoration of
// the classic SoQtExaminerViewer / SoXtExaminerViewer.
//
// The wheel does not know anything about cameras.  It simply emits
// wheelMoved(delta) where delta is the signed amount (in "radians of wheel
// rotation") the wheel was turned by.  Dragging towards increasing x (for a
// horizontal wheel) or increasing y, i.e. downwards (for a vertical wheel),
// yields positive deltas.

#ifndef THUMBWHEEL_H
#define THUMBWHEEL_H

#include <QWidget>

class ThumbWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ThumbWheel(Qt::Orientation orientation, QWidget* parent = nullptr);

    Qt::Orientation orientation() const { return m_orientation; }

    float value() const { return m_value; }
    void setValue(float v);                       // repaint only, no signal

    float radiansPerPixel() const { return m_radPerPixel; }
    void setRadiansPerPixel(float r) { m_radPerPixel = r; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void wheelPressed();
    void wheelMoved(float delta);
    void wheelReleased();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    Qt::Orientation m_orientation;
    float m_value = 0.0f;
    float m_radPerPixel = 0.02f;
    bool  m_dragging = false;
    QPoint m_lastPos;
};

#endif // THUMBWHEEL_H
