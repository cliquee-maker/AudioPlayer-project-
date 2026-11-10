#pragma once

#include <QPointF>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

// Круглая ручка громкости в духе автомобильной магнитолы.
// Вращение мышью (по кругу), колесо, стрелки; значение 0..100.
class VolumeDial : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
    explicit VolumeDial(QWidget* parent = nullptr);

    int value() const;
    int minimum() const;
    int maximum() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setValue(int value);
    void setRange(int minimum, int maximum);

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPointF center() const;
    qreal radius() const;
    double angleForValue(int value) const;
    double pointerAngle(const QPointF& position) const;

    int value_ = 100;
    int minimum_ = 0;
    int maximum_ = 100;
    bool dragging_ = false;
    double lastAngle_ = 0.0;
};
