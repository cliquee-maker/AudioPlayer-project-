#pragma once

// ============================================================================
// КОНТРАКТ (Этап 0) — MusicLibrary
// Библиотека треков: скан папки, хранение, выдача списка GUI.
// Изменения — только по согласованию всей команды!
// Реализацию (TagLib, фильтры расширений) пишет Алексей
// в src/library/MusicLibrary.cpp.
// ============================================================================

#include "library/Track.hpp"

#include <QObject>

#include <filesystem>
#include <vector>

class MusicLibrary : public QObject {
    Q_OBJECT
public:
    explicit MusicLibrary(QObject* parent = nullptr);
    ~MusicLibrary() override = default;

    // Рекурсивный скан каталога: mp3, wav, ogg, flac; чтение тегов.
    void scan(const std::filesystem::path& dir);

    // Поиск по title/artist делает GUI (QSortFilterProxyModel) — сюда
    // не встраиваем, чтобы контракт оставался минимальным.
    const std::vector<Track>& tracks() const;

    void clear();

signals:
    void libraryChanged();  // после scan()/clear()

private:
    std::vector<Track> tracks_;
};
