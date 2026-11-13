#pragma once

#include "gui/IVisualSource.hpp"

#include <QElapsedTimer>

#include <filesystem>
#include <vector>

class AudioEngine;
class QAudioBuffer;
class QAudioDecoder;

// Источник данных визуализатора. Пока трек не декодирован (или файл не
// поддерживается), работает детерминированная имитация по position/volume.
// Как только QAudioDecoder отдал PCM, спектр и осциллограмма считаются по
// реальному звуку файла, синхронно с позицией воспроизведения.

class SimulatedVisualSource : public IVisualSource {
public:
    explicit SimulatedVisualSource(AudioEngine& engine);
    ~SimulatedVisualSource() override;

    void setTrack(const std::filesystem::path& file);

    const std::vector<float>& bins() const override;
    const std::vector<float>& waveform() const override;
    bool isPlaying() const override;
    float volume() const override;
    qint64 positionMs() const override;
    qint64 durationMs() const override;

private:
    void startDecoder();
    void stopDecoder();
    void appendBuffer(const QAudioBuffer& buffer);

    bool hasRealAudio() const;
    void updateBinsFromPcm(float deltaMs) const;
    void updateBinsSynthetic(float deltaMs) const;
    // Плавная позиция для осциллографа: сигнал positionChanged от плеера
    // приходит редко, поэтому между обновлениями «докручиваем» её сами.
    qint64 advanceVisualPosition() const;
    void updateWaveformFromPcm(qint64 positionMs) const;
    void updateWaveformSynthetic(qint64 positionMs) const;

    AudioEngine& engine_;
    QAudioDecoder* decoder_ = nullptr;
    std::filesystem::path trackPath_;

    // Декодированный моно-PCM, приведённый к kAnalysisRate.
    std::vector<float> pcm_;
    qint64 inputFrames_ = 0;      // сколько кадров нативного PCM уже пришло
    qint64 nextOutputFrame_ = 0;  // следующий индекс в pcm_
    bool decodingFailed_ = false;

    mutable std::vector<float> bins_;
    mutable std::vector<float> waveform_;
    mutable std::vector<float> displayWave_;
    mutable QElapsedTimer clock_;
    mutable qint64 lastMs_ = -1;
    mutable double smoothPositionMs_ = -1.0;
    mutable qint64 lastWaveformTickMs_ = -1;
    mutable qint64 lastDisplayPositionMs_ = -1;
};
