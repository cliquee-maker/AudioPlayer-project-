#pragma once

// ============================================================================
// КОНТРАКТ (Этап 0) — Playlist
// Текущий плейлист: порядок треков, текущий индекс, режимы repeat/shuffle,
// сохранение/загрузка .plst (JSON через nlohmann).
// Изменения — только по согласованию всей команды!
// Реализацию пишет Алексей в src/library/Playlist.cpp.
// ============================================================================

#include "library/Track.hpp"

#include <QObject>

#include <filesystem>
#include <vector>

class Playlist : public QObject {
    Q_OBJECT
public:
    enum class Repeat { Off, Track, Playlist };
    Q_ENUM(Repeat)

    explicit Playlist(QObject* parent = nullptr);
    ~Playlist() override = default;

    void add(const Track& track);
    void insert(int index, const Track& track);
    void removeAt(int index);
    void move(int from, int to);  // reorder (drag-and-drop в GUI)
    void clear();

    int size() const;
    const Track& at(int index) const;

    int currentIndex() const;
    void setCurrentIndex(int index);
    const Track* current() const;  // nullptr, если плейлист пуст

    Repeat repeat() const;
    void setRepeat(Repeat mode);
    bool shuffle() const;
    void setShuffle(bool on);

    bool save(const std::filesystem::path& file) const;  // .plst (JSON)
    bool load(const std::filesystem::path& file);

signals:
    void changed();                 // состав/порядок изменился
    void currentChanged(int index); // сменился текущий трек

private:
    std::vector<Track> tracks_;
    int currentIndex_ = -1;
    Repeat repeat_ = Repeat::Off;
    bool shuffle_ = false;
};
