#include "library/Playlist.hpp"

#include "library/Track.hpp"

// Временная заглушка вместо src/library/Playlist.cpp — данные живут в памяти,
// save()/load() вернут false, пока не будет формат .plst на nlohmann.

Playlist::Playlist(QObject* parent) : QObject(parent) {}

void Playlist::add(const Track& track) {
    tracks_.push_back(track);
    emit changed();
}

void Playlist::insert(int index, const Track& track) {
    if (index < 0) index = 0;
    if (index > static_cast<int>(tracks_.size())) index = static_cast<int>(tracks_.size());
    tracks_.insert(tracks_.begin() + index, track);
    if (index <= currentIndex_) ++currentIndex_;
    emit changed();
}

void Playlist::removeAt(int index) {
    if (index < 0 || index >= static_cast<int>(tracks_.size())) return;
    const int before = currentIndex_;
    tracks_.erase(tracks_.begin() + index);
    if (index < currentIndex_) {
        --currentIndex_;
    } else if (index == currentIndex_) {
        if (currentIndex_ >= static_cast<int>(tracks_.size())) {
            currentIndex_ = tracks_.empty() ? -1 : static_cast<int>(tracks_.size()) - 1;
        }
    }
    emit changed();
    if (currentIndex_ != before) emit currentChanged(currentIndex_);
}

void Playlist::move(int from, int to) {
    const int size = static_cast<int>(tracks_.size());
    if (from < 0 || from >= size || to < 0 || to >= size || from == to) return;
    Track track = tracks_[from];
    tracks_.erase(tracks_.begin() + from);
    tracks_.insert(tracks_.begin() + to, track);

    const int before = currentIndex_;
    if (currentIndex_ == from) {
        currentIndex_ = to;
    } else if (from < currentIndex_ && to >= currentIndex_) {
        --currentIndex_;
    } else if (from > currentIndex_ && to <= currentIndex_) {
        ++currentIndex_;
    }
    emit changed();
    if (currentIndex_ != before) emit currentChanged(currentIndex_);
}

void Playlist::clear() {
    const int before = currentIndex_;
    tracks_.clear();
    currentIndex_ = -1;
    emit changed();
    if (before != -1) emit currentChanged(-1);
}

int Playlist::size() const {
    return static_cast<int>(tracks_.size());
}

const Track& Playlist::at(int index) const {
    return tracks_.at(static_cast<std::size_t>(index));
}

int Playlist::currentIndex() const {
    return currentIndex_;
}

void Playlist::setCurrentIndex(int index) {
    if (index == currentIndex_) return;
    if (index < -1 || index >= static_cast<int>(tracks_.size())) return;
    currentIndex_ = index;
    emit currentChanged(currentIndex_);
}

const Track* Playlist::current() const {
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(tracks_.size())) return nullptr;
    return &tracks_[static_cast<std::size_t>(currentIndex_)];
}

Playlist::Repeat Playlist::repeat() const {
    return repeat_;
}

void Playlist::setRepeat(Repeat mode) {
    repeat_ = mode;
}

bool Playlist::shuffle() const {
    return shuffle_;
}

void Playlist::setShuffle(bool on) {
    shuffle_ = on;
}

bool Playlist::save(const std::filesystem::path&) const {
    return false;
}

bool Playlist::load(const std::filesystem::path&) {
    return false;
}
