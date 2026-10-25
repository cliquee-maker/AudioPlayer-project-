#pragma once

// ============================================================================
// КОНТРАКТ (Этап 0) — Track
// Данные одного трека. Их заполняет MusicLibrary (теги через TagLib),
// GUI отображает. Изменения — только по согласованию всей команды!
// ============================================================================

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <filesystem>

struct Track {
    std::filesystem::path path;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    QByteArray coverData;  // JPEG/PNG из TagLib -> GUI грузит в QPixmap
};
