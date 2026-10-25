#include "library/MusicLibrary.hpp"

// Временная заглушка вместо src/library/MusicLibrary.cpp — отдаёт
// фиктивные треки, чтобы GUI можно было показывать без TagLib-скана.

MusicLibrary::MusicLibrary(QObject* parent) : QObject(parent) {}

void MusicLibrary::scan(const std::filesystem::path& dir) {
    tracks_.clear();

    struct Fake {
        const char* title;
        const char* artist;
        const char* album;
        qint64 durationMs;
        const char* file;
    };
    static const Fake fakes[] = {
        {"Night Drive", "Neon Echoes", "Synthwave", 213000, "01 - Night Drive.mp3"},
        {"Камерная", "Кварта", "Акустика", 187000, "02 - Камерная.mp3"},
        {"Blue Horizon", "Sea Salt", "Waves", 245000, "03 - Blue Horizon.mp3"},
        {"Тишина", "Орбита", "Эхо", 162000, "04 - Тишина.mp3"},
    };

    for (const Fake& f : fakes) {
        Track t;
        t.path = dir / f.file;
        t.title = QString::fromUtf8(f.title);
        t.artist = QString::fromUtf8(f.artist);
        t.album = QString::fromUtf8(f.album);
        t.durationMs = f.durationMs;
        tracks_.push_back(std::move(t));
    }

    emit libraryChanged();
}

const std::vector<Track>& MusicLibrary::tracks() const {
    return tracks_;
}

void MusicLibrary::clear() {
    tracks_.clear();
    emit libraryChanged();
}
