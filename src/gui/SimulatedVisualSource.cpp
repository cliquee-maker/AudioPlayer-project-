#include "gui/SimulatedVisualSource.hpp"

#include "audio/AudioEngine.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kBins = 48;
}

SimulatedVisualSource::SimulatedVisualSource(AudioEngine& engine)
    : engine_(engine), bins_(kBins, 0.0f) {
    clock_.start();
}

const std::vector<float>& SimulatedVisualSource::bins() const {
    const qint64 now = clock_.elapsed();
    qint64 delta = now - lastMs_;
    lastMs_ = now;
    if (delta <= 0 || delta > 250) delta = 16;

    const double t = static_cast<double>(engine_.positionMs()) / 1000.0;
    const float targetScale = engine_.isPlaying() ? engine_.volume() / 100.0f : 0.0f;

    for (int i = 0; i < kBins; ++i) {
        const double phase = i * 0.37;
        double v = 0.5 + 0.5 * std::sin(t * (1.7 + i * 0.06) + phase);
        v *= 0.55 + 0.45 * std::sin(t * 0.9 + i * 0.23 + 1.3);
        v = std::pow(std::max(v, 0.0), 1.4);
        const double tilt = 1.0 - 0.55 * (static_cast<double>(i) / kBins);
        const float target = static_cast<float>(v * tilt) * targetScale;

        const float tau = target > bins_[static_cast<std::size_t>(i)] ? 60.0f : 190.0f;
        const float k = 1.0f - std::exp(static_cast<float>(delta) / tau);
        bins_[static_cast<std::size_t>(i)] +=
            (target - bins_[static_cast<std::size_t>(i)]) * k;
    }
    return bins_;
}

bool SimulatedVisualSource::isPlaying() const {
    return engine_.isPlaying();
}

float SimulatedVisualSource::volume() const {
    return engine_.volume();
}

qint64 SimulatedVisualSource::positionMs() const {
    return engine_.positionMs();
}
