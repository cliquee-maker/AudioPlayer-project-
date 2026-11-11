#include "audio/AudioEngine.hpp"

#include <QAudio>
#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>

#include <algorithm>

AudioEngine::AudioEngine(QObject* parent) : QObject(parent) {
    // Плеер и аудиовыход — дочерние объекты движка, Qt удалит их сам
    player_ = new QMediaPlayer(this);
    output_ = new QAudioOutput(this);
    player_->setAudioOutput(output_);
    setVolume(volume_);

    // Позиция и длительность: сигналы плеера пробрасываем как есть
    connect(player_, &QMediaPlayer::positionChanged, this, &AudioEngine::positionChanged);
    connect(player_, &QMediaPlayer::durationChanged, this, &AudioEngine::durationChanged);

    // Состояние плеера (играет/пауза/стоп) -> простой флаг «играет»
    connect(player_, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                emit stateChanged(state == QMediaPlayer::PlayingState);
            });

    // Трек доиграл до конца -> GUI решает, что включать дальше
    connect(player_, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) emit mediaFinished();
            });

    // Ошибки декодирования/устройства отдаём наружу текстом
    connect(player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString& message) {
                if (error != QMediaPlayer::NoError) emit errorOccurred(message);
            });
}

bool AudioEngine::open(const std::filesystem::path& file) {
    // Проверяем файл сразу: QMediaPlayer грузит асинхронно и про
    // несуществующий путь сообщил бы только сигналом ошибки
    const QFileInfo info(file);
    if (!info.isFile() || !info.isReadable()) return false;

    // setSource останавливает плеер — если играли, продолжаем уже новый трек
    const bool resume = isPlaying();
    player_->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
    if (resume) player_->play();
    return true;
}

void AudioEngine::play() {
    player_->play();
}

void AudioEngine::pause() {
    player_->pause();
}

void AudioEngine::stop() {
    player_->stop();
}

void AudioEngine::seek(qint64 positionMs) {
    // Не даём уйти за границы трека
    const qint64 maxMs = std::max<qint64>(0, player_->duration());
    player_->setPosition(std::clamp<qint64>(positionMs, 0, maxMs));
}

void AudioEngine::setVolume(float v) {
    volume_ = std::clamp(v, 0.0f, 100.0f);
    // Ухо воспринимает громкость логарифмически: без перевода шкалы
    // вся регулировка сосредоточилась бы в начале ползунка
    output_->setVolume(QAudio::convertVolume(volume_ / 100.0f, QAudio::LogarithmicVolumeScale,
                                             QAudio::LinearVolumeScale));
}

float AudioEngine::volume() const {
    return volume_;
}

bool AudioEngine::isPlaying() const {
    return player_->playbackState() == QMediaPlayer::PlayingState;
}

qint64 AudioEngine::positionMs() const {
    return player_->position();
}

qint64 AudioEngine::durationMs() const {
    return player_->duration();
}

const std::vector<float>& AudioEngine::spectrum() const {
    return spectrum_;
}
