#include "gui/Visualizer.hpp"

#include "gui/IVisualSource.hpp"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>

Visualizer::Visualizer(IVisualSource* source, QWidget* parent)
    : QWidget(parent), source_(source) {
    setMinimumHeight(45);
    setMaximumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);

    auto* timer = new QTimer(this);
    timer->setInterval(16);
    connect(timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    timer->start();
}

Visualizer::Mode Visualizer::mode() const {
    return mode_;
}

void Visualizer::setMode(Mode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    emit modeChanged();
    update();
}

void Visualizer::cycleMode() {
    switch (mode_) {
    case Mode::Bars:
        mode_ = Mode::Oscilloscope;
        break;
    case Mode::Oscilloscope:
        mode_ = Mode::Bars;
        break;
    }
    emit modeChanged();
    update();
}

QString Visualizer::modeName() const {
    switch (mode_) {
    case Mode::Bars:
        return tr("Полосы");
    case Mode::Oscilloscope:
        return tr("Осциллограф");
    }
    return {};
}

void Visualizer::mouseDoubleClickEvent(QMouseEvent* event) {
    cycleMode();
    event->accept();
}

void Visualizer::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(18, 18, 18));
    if (source_ == nullptr) return;

    switch (mode_) {
    case Mode::Bars:
        paintBars(painter);
        break;
    case Mode::Oscilloscope:
        paintOscilloscope(painter);
        break;
    }
}

void Visualizer::paintBars(QPainter& painter) {
    const std::vector<float>& bins = source_->bins();
    const int count = static_cast<int>(bins.size());
    if (count == 0) return;

    if (peaks_.size() != count) peaks_.fill(0.0f, count);

    const qreal slot = width() / static_cast<qreal>(count);
    const qreal barW = std::max(2.0, slot * 0.68);
    const qreal heightMax = height() - 6.0;

    for (int i = 0; i < count; ++i) {
        const float value = std::clamp(bins[static_cast<std::size_t>(i)], 0.0f, 1.0f);
        const qreal barH = value * heightMax;
        const qreal x = i * slot + (slot - barW) / 2.0;
        const qreal y = height() - barH;

        QLinearGradient gradient(0, height(), 0, y);
        gradient.setColorAt(0.0, QColor(79, 140, 255));
        gradient.setColorAt(1.0, QColor(157, 93, 255));
        painter.fillRect(QRectF(x, y, barW, barH), gradient);

        float& peak = peaks_[i];
        peak = std::max(value, peak - 0.012f);
        const qreal peakY = height() - peak * heightMax;
        painter.fillRect(QRectF(x, peakY - 1.5, barW, 3.0), QColor(240, 240, 240));
    }
}

void Visualizer::paintOscilloscope(QPainter& painter) {
    const std::vector<float>& wave = source_->waveform();
    const int count = static_cast<int>(wave.size());
    if (count < 2) return;

    const qreal midY = height() / 2.0;
    const qreal amplitude = height() * 0.38;

    // Сглаженная кривая (Catmull-Rom): соседние отсчёты соединяются
    // кубическими сегментами, поэтому нет изломов на высоких частотах.
    QVector<QPointF> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        const qreal x = width() * static_cast<qreal>(i) / (count - 1);
        const qreal y = midY - wave[static_cast<std::size_t>(i)] * amplitude;
        points.append(QPointF(x, y));
    }

    const auto at = [&points, count](int index) {
        return points[std::clamp(index, 0, count - 1)];
    };

    QPainterPath path(points.first());
    for (int i = 0; i + 1 < count; ++i) {
        const QPointF p0 = at(i - 1);
        const QPointF p1 = at(i);
        const QPointF p2 = at(i + 1);
        const QPointF p3 = at(i + 2);
        const QPointF c1 = p1 + (p2 - p0) / 6.0;
        const QPointF c2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(c1, c2, p2);
    }

    QPen glow(QBrush(QColor(79, 140, 255, 80)), 7.0);
    glow.setCapStyle(Qt::RoundCap);
    glow.setJoinStyle(Qt::RoundJoin);
    painter.setPen(glow);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    QPen core(QBrush(QColor(235, 240, 255, 255)), 2.0);
    core.setCapStyle(Qt::RoundCap);
    core.setJoinStyle(Qt::RoundJoin);
    painter.setPen(core);
    painter.drawPath(path);
}
