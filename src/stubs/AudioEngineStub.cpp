#include "audio/AudioEngine.hpp"

#include <QTimer>

#include <algorithm>
#include <functional>

#include <audioproperties.h>
#include <fileref.h>

// Временная заглушка вместо src/audio/AudioEngine.cpp — убирается, когда
// Никита принесёт настоящую реализацию на QMediaPlayer/QAudioOutput.
// Да, никита перенесет
AudioEngine::AudioEngine(QObject* parent) : QObject(parent) {
    tick_ = new QTimer(this);
    tick_->setInterval(100);
    connect(tick_, &QTimer::timeout, this, [this] {
        if (!playing_) return;
        positionMs_ += tick_->interval();
        if (positionMs_ >= durationMs_) {
            positionMs_ = durationMs_;
            playing_ = false;
            tick_->stop();
            emit positionChanged(positionMs_);
            emit stateChanged(false);
            emit mediaFinished();
            return;
        }
        emit positionChanged(positionMs_);
    });
}

bool AudioEngine::open(const std::filesystem::path& file) {
    if (file.empty()) return false;
    const bool resume = playing_;
    playing_ = false;
    tick_->stop();
    positionMs_ = 0;
    // Реальная длительность из тега — та же, что в таблице библиотеки;
    // для файлов без свойств запасная формула (2..5 минут).
    durationMs_ = 0;
    const TagLib::FileRef media(file.c_str());
    if (!media.isNull() && media.audioProperties() != nullptr) {
        durationMs_ = media.audioProperties()->lengthInMilliseconds();
    }
    if (durationMs_ <= 0) {
        const auto h = std::hash<std::string>{}(file.string());
        durationMs_ = 120000 + static_cast<qint64>(h % 180001);
    }
    playing_ = resume;
    if (resume) tick_->start();
    emit durationChanged(durationMs_);
    emit positionChanged(positionMs_);
    emit stateChanged(playing_);
    return true;
}

void AudioEngine::play() {
    if (durationMs_ <= 0) return;
    if (positionMs_ >= durationMs_) positionMs_ = 0;
    playing_ = true;
    tick_->start();
    emit stateChanged(true);
    emit positionChanged(positionMs_);
}

void AudioEngine::pause() {
    playing_ = false;
    tick_->stop();
    emit stateChanged(false);
}

void AudioEngine::stop() {
    playing_ = false;
    tick_->stop();
    positionMs_ = 0;
    emit positionChanged(0);
    emit stateChanged(false);
}

void AudioEngine::seek(qint64 positionMs) {
    positionMs_ = std::clamp<qint64>(positionMs, 0, durationMs_);
    emit positionChanged(positionMs_);
}

void AudioEngine::setVolume(float v) {
    volume_ = std::clamp(v, 0.0f, 100.0f);
}

float AudioEngine::volume() const {
    return volume_;
}

bool AudioEngine::isPlaying() const {
    return playing_;
}

qint64 AudioEngine::positionMs() const {
    return positionMs_;
}

qint64 AudioEngine::durationMs() const {
    return durationMs_;
}

const std::vector<float>& AudioEngine::spectrum() const {
    return spectrum_;
}
