#pragma once

#include <QtGlobal>

#include <vector>

// Источник данных для визуализатора. Сейчас — SimulatedVisualSource,
// позже подменяется на FFT из AudioEngine::spectrum() без изменения UI.

struct IVisualSource {
    virtual ~IVisualSource() = default;

    virtual const std::vector<float>& bins() const = 0;      // 0..1, N полос
    virtual const std::vector<float>& waveform() const = 0;  // 256 отсчётов -1..1
    virtual bool isPlaying() const = 0;
    virtual float volume() const = 0;  // 0..100
    virtual qint64 positionMs() const = 0;
    virtual qint64 durationMs() const = 0;
};
