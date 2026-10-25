#pragma once

#include "gui/IVisualSource.hpp"

#include <QElapsedTimer>

class AudioEngine;

// Детерминированная имитация спектра из positionMs/volume/isPlaying:
// сумма синусов + сглаживание attack/release, затухание на паузе.

class SimulatedVisualSource : public IVisualSource {
public:
    explicit SimulatedVisualSource(AudioEngine& engine);

    const std::vector<float>& bins() const override;
    bool isPlaying() const override;
    float volume() const override;
    qint64 positionMs() const override;

private:
    AudioEngine& engine_;
    mutable std::vector<float> bins_;
    mutable QElapsedTimer clock_;
    mutable qint64 lastMs_ = -1;
};
