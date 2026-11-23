#include "widgets.h"
#include <QPainter>
#include <QtMath>
#include <algorithm>

Gauge::Gauge(const QString &title, const QString &unit, double min, double max, QWidget *parent)
    : QWidget(parent), m_title(title), m_unit(unit), m_min(min), m_max(max)
{
    setMinimumSize(160, 160);
}

void Gauge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int side = qMin(width(), height());
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(side / 200.0, side / 200.0);

    // dial: 240 degrees, from -210 (min) to +30 (max) in Qt angles
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 44, 52));
    p.drawEllipse(QPointF(0, 0), 95, 95);

    QRectF arc(-80, -80, 160, 160);
    QPen pen(QColor(90, 95, 105), 10, Qt::SolidLine, Qt::FlatCap);
    p.setPen(pen);
    p.drawArc(arc, 225 * 16, -270 * 16);

    double span = m_max - m_min;
    double frac = span > 0 ? std::clamp((m_value - m_min) / span, 0.0, 1.0) : 0;
    pen.setColor(QColor(80, 170, 255));
    p.setPen(pen);
    p.drawArc(arc, 225 * 16, int(-270 * 16 * frac));

    // ticks + labels
    p.setPen(QPen(Qt::white, 1));
    QFont f = p.font(); f.setPointSizeF(7); p.setFont(f);
    for (int i = 0; i <= 10; i++) {
        double a = qDegreesToRadians(225.0 - 27.0 * i);
        QPointF o(qCos(a) * 70, -qSin(a) * 70), in(qCos(a) * 62, -qSin(a) * 62);
        p.drawLine(in, o);
        if (i % 2 == 0) {
            QPointF t(qCos(a) * 50, -qSin(a) * 50);
            p.drawText(QRectF(t.x() - 20, t.y() - 7, 40, 14), Qt::AlignCenter,
                       QString::number(m_min + span * i / 10.0, 'g', 4));
        }
    }
    if (m_hasMarker && span > 0) {
        double mf = std::clamp((m_marker - m_min) / span, 0.0, 1.0);
        double a = qDegreesToRadians(225.0 - 270.0 * mf);
        p.setPen(QPen(QColor(255, 80, 80), 3));
        p.drawLine(QPointF(qCos(a) * 75, -qSin(a) * 75), QPointF(qCos(a) * 88, -qSin(a) * 88));
    }
    // needle
    double a = qDegreesToRadians(225.0 - 270.0 * frac);
    p.setPen(QPen(QColor(255, 170, 40), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(0, 0), QPointF(qCos(a) * 72, -qSin(a) * 72));
    p.setBrush(QColor(255, 170, 40));
    p.drawEllipse(QPointF(0, 0), 5, 5);

    // text
    p.setPen(Qt::white);
    f.setPointSizeF(9); f.setBold(true); p.setFont(f);
    p.drawText(QRectF(-90, 20, 180, 20), Qt::AlignCenter, QString::number(m_value, 'f', 1) + " " + m_unit);
    f.setBold(false); f.setPointSizeF(8); p.setFont(f);
    p.drawText(QRectF(-90, 45, 180, 18), Qt::AlignCenter, m_title);
}

TrendPlot::TrendPlot(const QString &title, QColor color, int capacity, QWidget *parent)
    : QWidget(parent), m_title(title), m_color(color), m_capacity(capacity)
{
    setMinimumHeight(110);
}

void TrendPlot::addSample(double v)
{
    m_data.append(v);
    if (m_data.size() > m_capacity) m_data.remove(0, m_data.size() - m_capacity);
    update();
}

void TrendPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 33, 40));
    p.setPen(QColor(200, 200, 200));
    p.drawText(6, 14, m_title);
    if (m_data.size() < 2) return;
    auto [mnIt, mxIt] = std::minmax_element(m_data.begin(), m_data.end());
    double mn = *mnIt, mx = *mxIt;
    if (mx - mn < 1e-6) { mx += 0.5; mn -= 0.5; }
    QRectF r(40, 20, width() - 48, height() - 28);
    p.setPen(QColor(150, 150, 150));
    p.drawText(QRectF(0, r.top() - 6, 38, 12), Qt::AlignRight, QString::number(mx, 'f', 2));
    p.drawText(QRectF(0, r.bottom() - 6, 38, 12), Qt::AlignRight, QString::number(mn, 'f', 2));
    p.setPen(QColor(60, 64, 72));
    p.drawRect(r);
    QPolygonF poly;
    for (int i = 0; i < m_data.size(); i++)
        poly << QPointF(r.left() + r.width() * i / (m_capacity - 1),
                        r.bottom() - r.height() * (m_data[i] - mn) / (mx - mn));
    p.setPen(QPen(m_color, 1.5));
    p.drawPolyline(poly);
}
