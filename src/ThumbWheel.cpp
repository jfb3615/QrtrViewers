// ThumbWheel.cpp

#include "QrtrViewers/ThumbWheel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtGlobal>
#include <cmath>

ThumbWheel::ThumbWheel(Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent), m_orientation(orientation)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(m_orientation == Qt::Horizontal ? Qt::SizeHorCursor
                                              : Qt::SizeVerCursor);
    if (m_orientation == Qt::Horizontal)
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    else
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize ThumbWheel::sizeHint() const
{
    return (m_orientation == Qt::Horizontal) ? QSize(110, 22) : QSize(22, 110);
}

QSize ThumbWheel::minimumSizeHint() const
{
    return sizeHint();
}

void ThumbWheel::setValue(float v)
{
    m_value = v;
    update();
}

void ThumbWheel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());

    const bool horiz = (m_orientation == Qt::Horizontal);
    const QRect r = rect().adjusted(2, 2, -3, -3);
    const int len = horiz ? r.width() : r.height();
    if (len <= 0)
        return;

    const double pi = 3.14159265358979323846;
    const double ridge = 0.30;          // angular distance between ridges

    // Draw the wheel one pixel-line at a time.  Each line corresponds to a
    // slice of the cylinder; brightness follows cos(angle) for a rounded look
    // and a groove is darkened periodically.  Subtracting m_value makes the
    // ridges travel in the direction the user is dragging.
    for (int i = 0; i < len; ++i) {
        const double t = (i + 0.5) / len;
        const double a = (t - 0.5) * pi * 0.92;
        const double shade = std::cos(a);
        double ph = std::fmod(a - double(m_value), ridge);
        if (ph < 0.0)
            ph += ridge;
        const double groove = (ph < ridge * 0.25) ? 0.45 : 1.0;
        const int g = qBound(0, int(30 + 205 * shade * groove), 255);

        p.setPen(QColor(g, g, g));
        if (horiz)
            p.drawLine(r.left() + i, r.top(), r.left() + i, r.bottom());
        else
            p.drawLine(r.left(), r.top() + i, r.right(), r.top() + i);
    }

    p.setPen(palette().color(QPalette::Dark));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(-1, -1, 0, 0));
}

void ThumbWheel::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_lastPos = e->pos();
    emit wheelPressed();
}

void ThumbWheel::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging)
        return;
    const QPoint d = e->pos() - m_lastPos;
    m_lastPos = e->pos();
    const int pixels = (m_orientation == Qt::Horizontal) ? d.x() : d.y();
    if (pixels == 0)
        return;
    const float delta = float(pixels) * m_radPerPixel;
    m_value += delta;
    update();
    emit wheelMoved(delta);
}

void ThumbWheel::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    emit wheelReleased();
}

void ThumbWheel::wheelEvent(QWheelEvent* e)
{
    const float steps = float(e->angleDelta().y()) / 120.0f;
    if (steps == 0.0f)
        return;
    const float delta = -steps * 0.25f;   // wheel up behaves like dragging up
    m_value += delta;
    update();
    emit wheelPressed();
    emit wheelMoved(delta);
    emit wheelReleased();
    e->accept();
}
