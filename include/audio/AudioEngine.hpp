#pragma once

// ============================================================================
// КОНТРАКТ (Этап 0) — AudioEngine
// Публичный API движка воспроизведения. GUI кодится строго против этого
// заголовка. Изменения — только по согласованию всей команды!
// Реализация (QMediaPlayer/QAudioOutput) — src/audio/AudioEngine.cpp.
// ============================================================================

#include <QObject>
#include <QString>

#include <filesystem>
#include <vector>

class QAudioOutput;
class QMediaPlayer;

class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override = default;

    // Подготовка файла к воспроизведению; false — файл не открылся.
    // Если в момент вызова что-то играло — новый трек сразу продолжит играть.
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
    // Файл не декодируется / нет аудиоустройства — текст ошибки от Qt.
    void errorOccurred(const QString& message);

private:
    QMediaPlayer* player_ = nullptr;  // декодирование и транспорт (play/pause/seek)
    QAudioOutput* output_ = nullptr;  // звуковое устройство и громкость
    float volume_ = 100.0f;           // громкость в шкале UI (0..100)
    std::vector<float> spectrum_;     // пока пустой — FFT подключим позже
};
