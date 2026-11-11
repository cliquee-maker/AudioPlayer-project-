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
#include <memory>
#include <vector>

class Database;

class MusicLibrary : public QObject {
    Q_OBJECT
public:
    explicit MusicLibrary(QObject* parent = nullptr);

    // Для тестов/встраивания: использовать готовое соединение с БД.
    // По умолчанию библиотека сама открывает Database::defaultPath().
    explicit MusicLibrary(Database& db, QObject* parent = nullptr);
    ~MusicLibrary() override;

    // Рекурсивный скан каталога: mp3, wav, ogg, flac; чтение тегов.
    void scan(const std::filesystem::path& dir);

    // Поиск по title/artist делает GUI (QSortFilterProxyModel) — сюда
    // не встраиваем, чтобы контракт оставался минимальным.
    const std::vector<Track>& tracks() const;

    void clear();

signals:
    void libraryChanged();  // после scan()/clear()

private:
    std::unique_ptr<Database> ownedDb_;  // создаётся только конструктором по умолчанию
    Database* db_ = nullptr;
    std::vector<Track> tracks_;
};
