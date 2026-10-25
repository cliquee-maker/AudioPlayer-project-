#include "stats/ListeningStats.hpp"

#include "library/Track.hpp"

#include <QDateTime>

#include <algorithm>
#include <map>

// Временная заглушка вместо src/stats/ListeningStats.cpp — события и запросы
// работают в памяти, save()/load() (stats.json) вернут false до реализации.

namespace {

qint64 periodStartMs(ListeningStats::Period period) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    switch (period) {
    case ListeningStats::Period::Last7Days:
        return now - 7LL * 24 * 60 * 60 * 1000;
    case ListeningStats::Period::Last30Days:
        return now - 30LL * 24 * 60 * 60 * 1000;
    case ListeningStats::Period::ThisYear: {
        QDateTime year = QDateTime::currentDateTime();
        year.setDate(QDate(QDateTime::currentDateTime().date().year(), 1, 1));
        year.setTime(QTime(0, 0));
        return year.toMSecsSinceEpoch();
    }
    case ListeningStats::Period::AllTime:
    default:
        return 0;
    }
}

}  // namespace

ListeningStats::ListeningStats(QObject* parent) : QObject(parent) {}

void ListeningStats::recordPlay(const Track& track, qint64 listenedMs) {
    recordPlay(track, listenedMs, QDateTime::currentMSecsSinceEpoch());
}

void ListeningStats::recordPlay(const Track& track, qint64 listenedMs, qint64 timestampMs) {
    Event event;
    event.timestampMs = timestampMs;
    event.path = track.path;
    event.artist = track.artist;
    event.listenedMs = listenedMs;
    events_.append(event);
    emit statsChanged();
}

QList<ListeningStats::ArtistStat> ListeningStats::topArtists(Period period, int n) const {
    const qint64 start = periodStartMs(period);
    std::map<QString, ArtistStat> acc;
    for (const Event& event : events_) {
        if (event.timestampMs < start) continue;
        ArtistStat& stat = acc[event.artist];
        stat.artist = event.artist;
        stat.totalMs += event.listenedMs;
        ++stat.playCount;
    }

    QList<ArtistStat> out;
    out.reserve(static_cast<qsizetype>(acc.size()));
    for (const auto& [name, stat] : acc) out.append(stat);
    std::sort(out.begin(), out.end(), [](const ArtistStat& a, const ArtistStat& b) {
        return a.totalMs != b.totalMs ? a.totalMs > b.totalMs : a.playCount > b.playCount;
    });
    if (n >= 0 && out.size() > n) out.resize(n);
    return out;
}

QList<ListeningStats::TrackStat> ListeningStats::topTracks(Period period, int n) const {
    const qint64 start = periodStartMs(period);
    std::map<std::filesystem::path, TrackStat> acc;
    for (const Event& event : events_) {
        if (event.timestampMs < start) continue;
        TrackStat& stat = acc[event.path];
        stat.path = event.path;
        stat.totalMs += event.listenedMs;
        ++stat.playCount;
        stat.lastPlayedMs = std::max(stat.lastPlayedMs, event.timestampMs);
    }

    QList<TrackStat> out;
    out.reserve(static_cast<qsizetype>(acc.size()));
    for (const auto& [path, stat] : acc) out.append(stat);
    std::sort(out.begin(), out.end(), [](const TrackStat& a, const TrackStat& b) {
        if (a.playCount != b.playCount) return a.playCount > b.playCount;
        return a.totalMs > b.totalMs;
    });
    if (n >= 0 && out.size() > n) out.resize(n);
    return out;
}

QList<ListeningStats::TrackStat> ListeningStats::recentlyPlayed(Period period, int n) const {
    const qint64 start = periodStartMs(period);
    std::map<std::filesystem::path, TrackStat> acc;
    for (const Event& event : events_) {
        if (event.timestampMs < start) continue;
        TrackStat& stat = acc[event.path];
        stat.path = event.path;
        stat.totalMs += event.listenedMs;
        ++stat.playCount;
        stat.lastPlayedMs = std::max(stat.lastPlayedMs, event.timestampMs);
    }

    QList<TrackStat> out;
    out.reserve(static_cast<qsizetype>(acc.size()));
    for (const auto& [path, stat] : acc) out.append(stat);
    std::sort(out.begin(), out.end(), [](const TrackStat& a, const TrackStat& b) {
        return a.lastPlayedMs > b.lastPlayedMs;
    });
    if (n >= 0 && out.size() > n) out.resize(n);
    return out;
}

int ListeningStats::playCountFor(const std::filesystem::path& file) const {
    int count = 0;
    for (const Event& event : events_) {
        if (event.path == file) ++count;
    }
    return count;
}

qint64 ListeningStats::totalListeningMs(Period period) const {
    const qint64 start = periodStartMs(period);
    qint64 total = 0;
    for (const Event& event : events_) {
        if (event.timestampMs >= start) total += event.listenedMs;
    }
    return total;
}

int ListeningStats::totalPlays(Period period) const {
    const qint64 start = periodStartMs(period);
    int total = 0;
    for (const Event& event : events_) {
        if (event.timestampMs >= start) ++total;
    }
    return total;
}

bool ListeningStats::save(const std::filesystem::path&) const {
    return false;
}

bool ListeningStats::load(const std::filesystem::path&) {
    return false;
}
