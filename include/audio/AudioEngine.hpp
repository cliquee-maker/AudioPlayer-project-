#pragma once

// ============================================================================
// КОНТРАКТ (Этап 0) — AudioEngine
// Публичный API движка воспроизведения. GUI кодится строго против этого
// заголовка. Изменения — только по согласованию всей команды!
// Реализацию (QMediaPlayer/QAudioOutput + приватные поля) пишет Никита
// в src/audio/AudioEngine.cpp.
// ============================================================================

#include <QObject>
#include <filesystem>
#include <vector>

class QTimer;

class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override = default;

    // Подготовка файла к воспроизведению; false — файл не открылся.
    bool open(const std::filesystem::path& file);

    void play();
    void pause();
    void stop();

    void seek(qint64 positionMs);

    void setVolume(float v);  // 0..100
    float volume() const;

    bool isPlaying() const;

    qint64 positionMs() const;
    qint64 durationMs() const;

    // Спектр для визуализатора: 0..1, N полос.
    // Сейчас: пусто/симуляция; позже — FFT. UI не меняется при подмене.
    const std::vector<float>& spectrum() const;

signals:
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void stateChanged(bool playing);
    void mediaFinished();

private:
    // Поля добавлены под заглушку (src/stubs/AudioEngineStub.cpp).
    // Никита заменяет их полями QMediaPlayer/QAudioOutput в своей реализации.
    QTimer* tick_ = nullptr;
    qint64 positionMs_ = 0;
    qint64 durationMs_ = 0;
    bool playing_ = false;
    float volume_ = 100.0f;
    std::vector<float> spectrum_;
};
