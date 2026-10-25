#pragma once

// ============================================================================
// КОНТРАКТ (Этап 0) — ListeningStats
// История прослушиваний для статистики «Spotify Wrapped» и умных плейлистов
// (Strawberry-style smart lists: «Most played», «Last played»).
// События хранятся как (timestampMs, путь, снимок artist, прослушано мс) —
// снимок имени артиста сохраняет статистику корректной даже если теги/файл
// изменятся позже.
// Изменения — только по согласованию всей команды!
// Реализацию (хранение, JSON через nlohmann) пишет Алексей
// в src/stats/ListeningStats.cpp.
//
// Схема подключения (делает Никита в App):
//   connect(engine, &AudioEngine::mediaFinished, ...) -> stats.recordPlay(...)
// GUI только читает запросы top*/recentlyPlayed и не участвует в записи.
// ============================================================================

#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <filesystem>

struct Track;  // include/library/Track.hpp — полный тип нужен только в .cpp

class ListeningStats : public QObject {
    Q_OBJECT
public:
    enum class Period { AllTime, Last7Days, Last30Days, ThisYear };
    Q_ENUM(Period)

    struct ArtistStat {
        QString artist;
        qint64 totalMs = 0;
        int playCount = 0;
    };

    struct TrackStat {
        std::filesystem::path path;
        qint64 totalMs = 0;
        int playCount = 0;
        qint64 lastPlayedMs = 0;
    };

    explicit ListeningStats(QObject* parent = nullptr);
    ~ListeningStats() override = default;

    // Запись события (время = текущее); listenedMs — реально прослушано.
    void recordPlay(const Track& track, qint64 listenedMs);

    // Явная метка времени — для тестов и импорта истории.
    void recordPlay(const Track& track, qint64 listenedMs, qint64 timestampMs);

    // Wrapped-экран: топ артистов за период.
    QList<ArtistStat> topArtists(Period period, int n) const;

    // Smart list «Most played»: GUI разрешает path -> Track через MusicLibrary.
    QList<TrackStat> topTracks(Period period, int n) const;

    // Smart list «Last played» (сортировка по lastPlayedMs убыв.).
    QList<TrackStat> recentlyPlayed(Period period, int n) const;

    int playCountFor(const std::filesystem::path& file) const;

    qint64 totalListeningMs(Period period) const;
    int totalPlays(Period period) const;

    bool save(const std::filesystem::path& file) const;  // stats.json
    bool load(const std::filesystem::path& file);

signals:
    void statsChanged();  // после recordPlay()/save()/load()

private:
    struct Event {
        qint64 timestampMs = 0;
        std::filesystem::path path;
        QString artist;
        qint64 listenedMs = 0;
    };

    QList<Event> events_;
};
