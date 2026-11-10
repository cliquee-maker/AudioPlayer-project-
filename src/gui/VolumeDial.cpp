#include "gui/VolumeDial.hpp"

#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kStartAngleDeg = 225.0;
constexpr double kSweepAngleDeg = -270.0;
constexpr double kStartAngleRad = kStartAngleDeg * kPi / 180.0;
constexpr double kSweepAngleRad = kSweepAngleDeg * kPi / 180.0;

QPointF pointOnCircle(const QPointF& center, qreal radius, double degrees) {
    const double radians = degrees * kPi / 180.0;
    return {center.x() + radius * std::cos(radians),
            center.y() - radius * std::sin(radians)};
}

}  // namespace

VolumeDial::VolumeDial(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(tr("Громкость"));
    setCursor(Qt::PointingHandCursor);
}

int VolumeDial::value() const {
    return value_;
}

int VolumeDial::minimum() const {
    return minimum_;
}

int VolumeDial::maximum() const {
    return maximum_;
}

QSize VolumeDial::sizeHint() const {
    return {88, 88};
}

QSize VolumeDial::minimumSizeHint() const {
    return {64, 64};
}

void VolumeDial::setValue(int value) {
    const int clamped = std::clamp(value, minimum_, maximum_);
    if (clamped == value_) return;
    value_ = clamped;
    emit valueChanged(value_);
    update();
}

void VolumeDial::setRange(int minimum, int maximum) {
    if (minimum > maximum) std::swap(minimum, maximum);
    minimum_ = minimum;
    maximum_ = maximum;
    setValue(value_);
    update();
}

QPointF VolumeDial::center() const {
    return {width() / 2.0, height() / 2.0};
}

qreal VolumeDial::radius() const {
    return std::min(width(), height()) / 2.0 - 3.0;
}

double VolumeDial::angleForValue(int value) const {
    const double span = maximum_ - minimum_;
    const double fraction = span > 0.0 ? (value - minimum_) / span : 0.0;
    return kStartAngleRad + kSweepAngleRad * fraction;
}

double VolumeDial::pointerAngle(const QPointF& position) const {
    const QPointF c = center();
    return std::atan2(c.y() - position.y(), position.x() - c.x());
}

void VolumeDial::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF c = center();
    const qreal r = radius();
    if (r <= 4.0) return;

    const qreal arcRadius = r * 0.80;
    const qreal arcWidth = std::max(3.0, r * 0.105);
    const QRectF arcRect(c.x() - arcRadius, c.y() - arcRadius,
                         arcRadius * 2.0, arcRadius * 2.0);

    // Риски по окружности, как на корпусе магнитолы.
    const int tickCount = 21;
    for (int i = 0; i < tickCount; ++i) {
        const double fraction = i / static_cast<double>(tickCount - 1);
        const double degrees = kStartAngleDeg + kSweepAngleDeg * fraction;
        const bool major = (i % 5 == 0);
        QPen pen(major ? QColor(185, 190, 200, 190) : QColor(115, 120, 132, 130),
                 major ? 1.6 : 1.0);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(pointOnCircle(c, r * (major ? 0.88 : 0.92), degrees),
                         pointOnCircle(c, r * 0.99, degrees));
    }

    // Фоновая шкала.
    QPen trackPen(QColor(48, 50, 58), arcWidth);
    trackPen.setCapStyle(Qt::RoundCap);
    painter.setPen(trackPen);
    painter.drawArc(arcRect, static_cast<int>(kStartAngleDeg * 16.0),
                    static_cast<int>(kSweepAngleDeg * 16.0));

    // Заполненная часть шкалы.
    const double span = maximum_ - minimum_;
    const double fraction =
        span > 0.0 ? std::clamp((value_ - minimum_) / span, 0.0, 1.0) : 0.0;
    if (fraction > 0.0) {
        QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
        gradient.setColorAt(0.0, QColor(79, 140, 255));
        gradient.setColorAt(1.0, QColor(157, 93, 255));
        QPen activePen(QBrush(gradient), arcWidth);
        activePen.setCapStyle(Qt::RoundCap);
        painter.setPen(activePen);
        painter.drawArc(arcRect, static_cast<int>(kStartAngleDeg * 16.0),
                        static_cast<int>(kSweepAngleDeg * fraction * 16.0));
    }

    // Сама ручка.
    const qreal knobRadius = r * 0.60;
    QRadialGradient knobGradient(c.x() - knobRadius * 0.35,
                                 c.y() - knobRadius * 0.40, knobRadius * 1.7);
    knobGradient.setColorAt(0.0, QColor(88, 90, 100));
    knobGradient.setColorAt(0.55, QColor(46, 47, 54));
    knobGradient.setColorAt(1.0, QColor(18, 19, 23));

    painter.setPen(QPen(QColor(12, 12, 15), 1.5));
    painter.setBrush(knobGradient);
    painter.drawEllipse(c, knobRadius, knobRadius);

    painter.setPen(QPen(QColor(110, 112, 124, 200), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(c, knobRadius, knobRadius);

    // Метка положения ручки.
    const double angle = angleForValue(value_);
    QPen indicatorPen(QColor(235, 240, 255), std::max(2.0, r * 0.035));
    indicatorPen.setCapStyle(Qt::RoundCap);
    painter.setPen(indicatorPen);
    painter.drawLine(pointOnCircle(c, knobRadius * 0.55, angle * 180.0 / kPi),
                     pointOnCircle(c, knobRadius * 0.94, angle * 180.0 / kPi));

    // Значение в центре.
    QFont valueFont = font();
    valueFont.setBold(true);
    valueFont.setPointSizeF(std::max(8.0, r * 0.22));
    painter.setFont(valueFont);
    painter.setPen(QColor(235, 238, 245));
    painter.drawText(QRectF(c.x() - knobRadius * 0.5,
                            c.y() - knobRadius * 0.5,
                            knobRadius, knobRadius),
                     Qt::AlignCenter, QString::number(value_));

    if (hasFocus()) {
        QPen focusPen(QColor(79, 140, 255, 200), 1.2, Qt::DashLine);
        painter.setPen(focusPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(c, r - 0.5, r - 0.5);
    }
}

void VolumeDial::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        lastAngle_ = pointerAngle(event->position());
        setFocus(Qt::MouseFocusReason);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void VolumeDial::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPointF c = center();
    if (std::hypot(event->position().x() - c.x(),
                   event->position().y() - c.y()) < radius() * 0.2) {
        lastAngle_ = pointerAngle(event->position());
        return;
    }

    const double current = pointerAngle(event->position());
    const double delta = std::remainder(current - lastAngle_, 2.0 * kPi);
    lastAngle_ = current;

    if (std::abs(delta) < 1e-5) return;

    const double span = std::abs(kSweepAngleRad);
    const int deltaValue = static_cast<int>(
        std::lround(-delta / span * (maximum_ - minimum_)));
    if (deltaValue != 0) setValue(value_ + deltaValue);
    event->accept();
}

void VolumeDial::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void VolumeDial::wheelEvent(QWheelEvent* event) {
    const int notches = event->angleDelta().y();
    if (notches == 0) {
        event->ignore();
        return;
    }

    const int range = maximum_ - minimum_;
    const int step = std::max(1, range / 20);
    setValue(value_ + (notches > 0 ? step : -step));
    event->accept();
}

void VolumeDial::keyPressEvent(QKeyEvent* event) {
    const int range = maximum_ - minimum_;
    const int step = std::max(1, range / 100);

    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Right:
        setValue(value_ + step);
        event->accept();
        return;
    case Qt::Key_Down:
    case Qt::Key_Left:
        setValue(value_ - step);
        event->accept();
        return;
    case Qt::Key_PageUp:
        setValue(value_ + std::max(1, range / 10));
        event->accept();
        return;
    case Qt::Key_PageDown:
        setValue(value_ - std::max(1, range / 10));
        event->accept();
        return;
    case Qt::Key_Home:
        setValue(minimum_);
        event->accept();
        return;
    case Qt::Key_End:
        setValue(maximum_);
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}
