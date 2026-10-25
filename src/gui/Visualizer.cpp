#include "gui/Visualizer.hpp"

#include "gui/IVisualSource.hpp"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

#include <algorithm>

Visualizer::Visualizer(IVisualSource* source, QWidget* parent)
    : QWidget(parent), source_(source) {
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* timer = new QTimer(this);
    timer->setInterval(16);
    connect(timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    timer->start();
}

Visualizer::Mode Visualizer::mode() const {
    return mode_;
}

void Visualizer::setMode(Mode mode) {
    mode_ = mode;
    update();
}

void Visualizer::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(18, 18, 18));
    if (source_ == nullptr) return;

    switch (mode_) {
    case Mode::Bars:
        paintBars(painter);
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
