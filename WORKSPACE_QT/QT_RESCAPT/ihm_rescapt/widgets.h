#ifndef WIDGETS_H
#define WIDGETS_H
#include <QWidget>
#include <QVector>
#include <QColor>

// Circular gauge (needle) widget
class Gauge : public QWidget
{
    Q_OBJECT
public:
    Gauge(const QString &title, const QString &unit, double min, double max, QWidget *parent = nullptr);
    void setValue(double v) { m_value = v; update(); }
    void setRange(double min, double max) { m_min = min; m_max = max; update(); }
    void setMarker(double m) { m_marker = m; m_hasMarker = true; update(); }
    void setTitle(const QString &t, const QString &u) { m_title = t; m_unit = u; update(); }
    QSize sizeHint() const override { return QSize(200, 200); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_title, m_unit;
    double m_min, m_max, m_value = 0, m_marker = 0;
    bool m_hasMarker = false;
};

// Scrolling time plot (last N samples)
class TrendPlot : public QWidget
{
    Q_OBJECT
public:
    TrendPlot(const QString &title, QColor color, int capacity = 300, QWidget *parent = nullptr);
    void addSample(double v);
    QSize sizeHint() const override { return QSize(300, 130); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_title;
    QColor m_color;
    int m_capacity;
    QVector<double> m_data;
};
#endif
