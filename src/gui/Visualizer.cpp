#include "gui/Visualizer.hpp"

#include "gui/IVisualSource.hpp"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

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
        mode_ = Mode::Authentic;
        break;
    case Mode::Authentic:
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
    case Mode::Authentic:
        return QStringLiteral("Authentic");
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
    case Mode::Authentic:
        paintAuthentic(painter);
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

    QPainterPath path;
    for (int i = 0; i < count; ++i) {
        const qreal x = width() * static_cast<qreal>(i) / (count - 1);
        const qreal y = midY - wave[static_cast<std::size_t>(i)] * amplitude;
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
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

void Visualizer::paintAuthentic(QPainter& painter) {
    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;
    const qreal R = std::min(width(), height()) * 0.38;
    if (R < 20.0) return;

    const qreal pos = static_cast<qreal>(source_->positionMs());
    const qreal dur = std::max<qreal>(static_cast<qreal>(source_->durationMs()), 1.0);
    const qreal progress = std::clamp(pos / dur, 0.0, 1.0);
    // 33⅓ об/мин: 200°/с
    const qreal angleDeg = (pos / 1000.0) * 200.0;

    const auto& bins = source_->bins();
    qreal bass = 0.0;
    const int binCount = static_cast<int>(bins.size());
    for (int i = 0; i < 5 && i < binCount; ++i) bass += bins[static_cast<std::size_t>(i)];
    if (binCount > 0) bass /= std::min(5, binCount);
    const qreal vol = source_->volume() / 100.0;

    // Свечение по окружности от басов
    const qreal glowAlpha = std::min(230.0, 50.0 + (90.0 + 90.0 * bass) * vol);
    QPen glow(QBrush(QColor(79, 140, 255, static_cast<int>(glowAlpha))), R * 0.035);
    painter.setPen(glow);
    painter.setBrush(Qt::NoPen);
    painter.drawEllipse(QPointF(cx, cy), R * 1.05 + bass * R * 0.04,
                        R * 1.05 + bass * R * 0.04);

    // Корпус диска
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 10, 10));
    painter.drawEllipse(QPointF(cx, cy), R, R);

    painter.save();
    painter.translate(cx, cy);
    painter.rotate(angleDeg);

    // 120 радиальных насечек по бортику — видимое вращение
    for (int i = 0; i < 120; ++i) {
        painter.save();
        painter.rotate(i * 3.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush((i % 3 == 0) ? QColor(58, 58, 64) : QColor(38, 38, 42));
        painter.drawRect(QRectF(-1.2, -R, 2.4, R * 0.05));
        painter.restore();
    }

    // Канавки
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 25; ++i) {
        const qreal rr = R * (0.40 + 0.55 * i / 24.0);
        painter.setPen(QPen(QBrush(QColor(255, 255, 255, (i % 2 == 0) ? 26 : 12)), 1.0));
        painter.drawEllipse(QPointF(0.0, 0.0), rr, rr);
    }

    // Вращающиеся блики (асимметрия — без них круг не читается как вращающийся)
    const auto sheen = [&](qreal rr, qreal a0, qreal span, int alpha, qreal widthF) {
        QPainterPath path;
        path.arcTo(QRectF(-rr, -rr, 2.0 * rr, 2.0 * rr), a0, span);
        QPen pen(QBrush(QColor(255, 255, 255, alpha)), widthF);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    };
    sheen(R * 0.72, 20.0, 70.0, 30, R * 0.16);
    sheen(R * 0.87, 35.0, 45.0, 22, R * 0.06);
    sheen(R * 0.52, 205.0, 60.0, 16, R * 0.10);

    // Центральная этикетка
    const qreal rLab = R * 0.33;
    QLinearGradient label(-rLab, -rLab, rLab, rLab);
    label.setColorAt(0.0, QColor(79, 140, 255));
    label.setColorAt(1.0, QColor(157, 93, 255));
    painter.setPen(QPen(QBrush(QColor(255, 255, 255, 70)), 1.0));
    painter.setBrush(label);
    painter.drawEllipse(QPointF(0.0, 0.0), rLab, rLab);

    // Метка off-center — вращается вместе с этикеткой
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 190));
    painter.drawEllipse(QPointF(rLab * 0.5, -rLab * 0.3), R * 0.03, R * 0.03);

    // Шпиндель
    painter.setPen(QPen(QBrush(QColor(240, 240, 240)), 1.5));
    painter.setBrush(QColor(18, 18, 18));
    painter.drawEllipse(QPointF(0.0, 0.0), R * 0.03, R * 0.03);

    painter.restore();

    // Тонарм: головка идёт от внешнего края к центру по прогрессу трека
    const qreal kArmCos = std::cos(-kPi / 4.0);  // направление 45° вправо-вверх
    const qreal kArmSin = std::sin(-kPi / 4.0);
    const QPointF pivot(cx + R * 0.98, cy - R * 0.98);
    const qreal dist = R * (0.94 - 0.52 * progress);
    const QPointF head(cx + kArmCos * dist, cy + kArmSin * dist);

    QPen arm(QBrush(QColor(165, 165, 175)), std::max(2.0, R * 0.03));
    arm.setCapStyle(Qt::RoundCap);
    painter.setPen(arm);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(pivot, head);

    painter.save();
    painter.translate(head);
    painter.rotate(45.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(215, 215, 222));
    painter.drawRoundedRect(QRectF(-R * 0.04, -R * 0.028, R * 0.11, R * 0.056), 3.0, 3.0);
    painter.restore();

    painter.setPen(QPen(QBrush(QColor(130, 130, 140)), 1.5));
    painter.setBrush(QColor(55, 55, 60));
    painter.drawEllipse(pivot, R * 0.075, R * 0.075);
}
