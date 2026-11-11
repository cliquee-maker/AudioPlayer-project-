#include "library/MusicLibrary.hpp"

#include "db/Database.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QtGlobal>
#include <QDebug>

#include <attachedpictureframe.h>
#include <audioproperties.h>
#include <fileref.h>
#include <flacfile.h>
#include <flacpicture.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <tag.h>
#include <tstring.h>
#include <vorbisfile.h>
#include <wavfile.h>
#include <xiphcomment.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

// Реализация MusicLibrary (этап 2): инкрементальный кэш библиотеки в SQLite.
// При повторном скане неизменившиеся файлы (mtime + size) не перечитываются
// TagLib-ом; обложки складываются файлами в AppDataLocation/covers, в БД
// хранится только путь.

namespace {

constexpr int kCoverFileNameHashLength = 20;

std::atomic<int> g_libraryConnectionCounter{0};

bool isAudioFile(const std::filesystem::path& path) {
    static const QSet<QString> kExtensions = {
        QStringLiteral("mp3"),
        QStringLiteral("wav"),
        QStringLiteral("ogg"),
        QStringLiteral("flac"),
    };

    QString extension = QString::fromStdString(path.extension().string()).toLower();
    if (extension.startsWith(QLatin1Char('.'))) extension.remove(0, 1);
    return kExtensions.contains(extension);
}

QString toQString(const TagLib::String& string) {
    return QString::fromUtf8(string.toCString(true));
}

QByteArray toQByteArray(const TagLib::ByteVector& bytes) {
    return QByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

QString pathToQString(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    // На Linux/macOS std::filesystem::path хранит байты, для нас это UTF-8.
    return QString::fromUtf8(path.u8string().c_str());
#endif
}

std::filesystem::path qStringToPath(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toUtf8().constData());
#endif
}

std::filesystem::path canonicalPath(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path result = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return result;

    ec.clear();
    result = std::filesystem::absolute(path, ec);
    if (!ec) return result;

    return path;
}

qint64 fileMtimeMs(const std::filesystem::path& path) {
    // QFileInfo стабильно отдаёт mtime в мс (std::filesystem::file_clock в
    // C++17 не конвертируется в system_clock без clock_cast из C++20).
    const QFileInfo info(pathToQString(path));
    if (!info.exists()) return 0;
    return info.lastModified().toMSecsSinceEpoch();
}

qint64 fileSize(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<qint64>(size);
}

std::vector<std::filesystem::path> collectAudioFiles(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return files;

    std::filesystem::recursive_directory_iterator it(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;

    while (!ec && it != end) {
        std::error_code fileEc;
        if (it->is_regular_file(fileEc) && isAudioFile(it->path())) {
            files.push_back(it->path());
        }
        it.increment(ec);
    }

    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return pathToQString(a) < pathToQString(b);
    });
    return files;
}

// APIC-кадры ID3v2 (mp3, wav): предпочитаем FrontCover, иначе первый кадр.
QByteArray coverFromId3(TagLib::ID3v2::Tag* tag) {
    if (tag == nullptr) return {};
    QByteArray fallback;
    const TagLib::ID3v2::FrameList& frames = tag->frameList("APIC");
    for (const TagLib::ID3v2::Frame* raw : frames) {
        const auto* picture =
            dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(raw);
        if (picture == nullptr || picture->picture().size() == 0) continue;
        if (picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
            return toQByteArray(picture->picture());
        }
        if (fallback.isEmpty()) fallback = toQByteArray(picture->picture());
    }
    return fallback;
}

// Картинки FLAC/OGG (METADATA_BLOCK_PICTURE): тот же принцип отбора.
QByteArray coverFromPictures(const TagLib::List<TagLib::FLAC::Picture*>& pictures) {
    QByteArray fallback;
    for (const TagLib::FLAC::Picture* picture : pictures) {
        if (picture == nullptr || picture->data().size() == 0) continue;
        if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
            return toQByteArray(picture->data());
        }
        if (fallback.isEmpty()) fallback = toQByteArray(picture->data());
    }
    return fallback;
}

QByteArray readCover(const std::filesystem::path& path) {
    const QString extension = QString::fromStdString(path.extension().string()).toLower();
    if (extension == QStringLiteral(".mp3")) {
        TagLib::MPEG::File file(path.c_str());
        return file.isValid() ? coverFromId3(file.ID3v2Tag()) : QByteArray();
    }
    if (extension == QStringLiteral(".flac")) {
        TagLib::FLAC::File file(path.c_str());
        return file.isValid() ? coverFromPictures(file.pictureList()) : QByteArray();
    }
    if (extension == QStringLiteral(".wav")) {
        TagLib::RIFF::WAV::File file(path.c_str());
        return file.isValid() ? coverFromId3(file.ID3v2Tag()) : QByteArray();
    }
    if (extension == QStringLiteral(".ogg")) {
        TagLib::Vorbis::File file(path.c_str());
        if (!file.isValid() || file.tag() == nullptr) return {};
        return coverFromPictures(file.tag()->pictureList());
    }
    return {};
}

Track readTrackMetadata(const std::filesystem::path& path) {
    Track track;
    track.path = canonicalPath(path);

    const TagLib::FileRef media(track.path.c_str());
    if (!media.isNull()) {
        if (const TagLib::Tag* tag = media.tag()) {
            track.title = toQString(tag->title());
            track.artist = toQString(tag->artist());
            track.album = toQString(tag->album());
            track.genre = toQString(tag->genre()).trimmed();
        }
        if (const TagLib::AudioProperties* properties = media.audioProperties()) {
            track.durationMs = properties->lengthInMilliseconds();
        }
    }

    if (track.title.trimmed().isEmpty()) {
        track.title = pathToQString(track.path.stem());
    }
    track.artist = track.artist.trimmed();
    track.album = track.album.trimmed();
    track.coverData = readCover(track.path);
    return track;
}

QString coverExtension(const QByteArray& data) {
    if (data.size() >= 3 && static_cast<unsigned char>(data.at(0)) == 0xFF &&
        static_cast<unsigned char>(data.at(1)) == 0xD8 &&
        static_cast<unsigned char>(data.at(2)) == 0xFF) {
        return QStringLiteral(".jpg");
    }
    if (data.size() >= 8 && data.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) {
        return QStringLiteral(".png");
    }
    if (data.size() >= 4 && data.startsWith("GIF8")) return QStringLiteral(".gif");
    if (data.size() >= 12 && data.startsWith("RIFF") &&
        data.mid(8, 4) == QByteArray("WEBP", 4)) {
        return QStringLiteral(".webp");
    }
    if (data.size() >= 2 && data.startsWith("BM")) return QStringLiteral(".bmp");
    return QStringLiteral(".img");
}

// Кладём обложку в AppDataLocation/covers/<sha1(путь)>.<ext> и возвращаем
// путь для колонки tracks.cover_path. BLOB в БД не пишем.
QString storeCover(const QByteArray& data, const QString& trackPath) {
    if (data.isEmpty()) return {};

    const QString coversDir = Database::coversDir();
    if (!QDir().mkpath(coversDir)) {
        qWarning() << "MusicLibrary: не удалось создать каталог обложек" << coversDir;
        return {};
    }

    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(trackPath.toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(kCoverFileNameHashLength));
    const QString coverPath =
        QDir(coversDir).filePath(hash + coverExtension(data));

    QSaveFile file(coverPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
        !file.commit()) {
        qWarning() << "MusicLibrary: не удалось записать обложку" << coverPath;
        return {};
    }
    return coverPath;
}

QByteArray loadCover(const QString& coverPath) {
    if (coverPath.isEmpty()) return {};
    QFile file(coverPath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

QVariant idByName(const QSqlDatabase& db, const QString& table, const QString& name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return {};

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral("INSERT OR IGNORE INTO %1(name) VALUES (?)").arg(table));
    insert.addBindValue(trimmed);
    if (!insert.exec()) {
        qWarning() << "MusicLibrary: не удалось добавить" << table << insert.lastError().text();
        return {};
    }

    QSqlQuery select(db);
    select.prepare(QStringLiteral("SELECT id FROM %1 WHERE name = ? LIMIT 1").arg(table));
    select.addBindValue(trimmed);
    if (!select.exec() || !select.next()) {
        qWarning() << "MusicLibrary: не удалось найти" << table << trimmed;
        return {};
    }
    return select.value(0);
}

QVariant albumId(const QSqlDatabase& db, const QString& title, const QVariant& artistId) {
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) return {};

    QSqlQuery select(db);
    select.prepare(QStringLiteral(
        "SELECT id FROM albums WHERE title = ? AND IFNULL(artist_id, -1) = IFNULL(?, -1) "
        "LIMIT 1"));
    select.addBindValue(trimmed);
    select.addBindValue(artistId);
    if (select.exec() && select.next()) return select.value(0);

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral("INSERT INTO albums(title, artist_id) VALUES (?, ?)"));
    insert.addBindValue(trimmed);
    insert.addBindValue(artistId);
    if (!insert.exec()) {
        qWarning() << "MusicLibrary: не удалось добавить альбом" << insert.lastError().text();
        return {};
    }
    return insert.lastInsertId();
}

void loadTracksFromDatabase(const QSqlDatabase& db, std::vector<Track>& tracks) {
    tracks.clear();

    QSqlQuery query(db);
    const bool ok = query.exec(QStringLiteral(
        "SELECT t.path, t.title, IFNULL(a.name, ''), IFNULL(al.title, ''), "
        "       IFNULL(g.name, ''), t.duration_ms, t.cover_path "
        "FROM tracks t "
        "LEFT JOIN artists a ON a.id = t.artist_id "
        "LEFT JOIN albums  al ON al.id = t.album_id "
        "LEFT JOIN genres  g ON g.id = t.genre_id "
        "ORDER BY t.path"));
    if (!ok) {
        qWarning() << "MusicLibrary: не удалось загрузить треки:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        Track track;
        track.path = qStringToPath(query.value(0).toString());
        track.title = query.value(1).toString();
        track.artist = query.value(2).toString();
        track.album = query.value(3).toString();
        track.genre = query.value(4).toString();
        track.durationMs = query.value(5).toLongLong();
        track.coverData = loadCover(query.value(6).toString());
        tracks.push_back(std::move(track));
    }
}

void scanWithoutDatabase(const std::filesystem::path& dir, std::vector<Track>& tracks) {
    tracks.clear();
    for (const auto& file : collectAudioFiles(dir)) {
        tracks.push_back(readTrackMetadata(file));
    }
}

}  // namespace

MusicLibrary::MusicLibrary(QObject* parent)
    : QObject(parent),
      ownedDb_(std::make_unique<Database>(QStringLiteral("audioplayer-library-%1")
                                              .arg(++g_libraryConnectionCounter))) {
    db_ = ownedDb_.get();
}

MusicLibrary::MusicLibrary(Database& db, QObject* parent)
    : QObject(parent), db_(&db) {}

MusicLibrary::~MusicLibrary() = default;

void MusicLibrary::scan(const std::filesystem::path& dir) {
    if (db_ == nullptr || !db_->isOpen()) {
        if (db_ == nullptr || !db_->open()) {
            qWarning() << "MusicLibrary: БД недоступна, скан без кэша";
            scanWithoutDatabase(dir, tracks_);
            emit libraryChanged();
            return;
        }
    }
    if (!db_->migrate()) {
        qWarning() << "MusicLibrary: миграция БД не удалась, скан без кэша";
        scanWithoutDatabase(dir, tracks_);
        emit libraryChanged();
        return;
    }

    const QSqlDatabase db = db_->handle();
    if (!db_->beginTransaction()) {
        qWarning() << "MusicLibrary: не удалось начать транзакцию скана";
        scanWithoutDatabase(dir, tracks_);
        emit libraryChanged();
        return;
    }

    // Что уже есть в БД: путь -> (id, mtime, size).
    struct KnownTrack {
        qint64 id = 0;
        qint64 mtime = 0;
        qint64 size = 0;
    };
    QHash<QString, KnownTrack> known;
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT path, id, file_mtime, file_size FROM tracks"))) {
            while (query.next()) {
                known.insert(query.value(0).toString(),
                             KnownTrack{query.value(1).toLongLong(),
                                        query.value(2).toLongLong(),
                                        query.value(3).toLongLong()});
            }
        } else {
            qWarning() << "MusicLibrary: не удалось прочитать кэш треков:"
                       << query.lastError().text();
        }
    }

    QSqlQuery insertTrack(db);
    insertTrack.prepare(QStringLiteral(
        "INSERT INTO tracks(path, title, artist_id, album_id, genre_id, duration_ms, "
        "                   cover_path, file_mtime, file_size, added_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery updateTrack(db);
    updateTrack.prepare(QStringLiteral(
        "UPDATE tracks SET title = ?, artist_id = ?, album_id = ?, genre_id = ?, "
        "                  duration_ms = ?, cover_path = ?, file_mtime = ?, file_size = ? "
        "WHERE id = ?"));

    QSet<QString> scannedPaths;
    for (const auto& file : collectAudioFiles(dir)) {
        const std::filesystem::path canonical = canonicalPath(file);
        const QString pathKey = pathToQString(canonical);
        if (scannedPaths.contains(pathKey)) continue;
        scannedPaths.insert(pathKey);

        const qint64 mtime = fileMtimeMs(canonical);
        const qint64 size = fileSize(canonical);

        const auto knownIt = known.constFind(pathKey);
        if (knownIt != known.constEnd() && knownIt->mtime == mtime &&
            knownIt->size == size) {
            // Главная экономия: файл не изменился — TagLib не трогаем.
            continue;
        }

        Track track = readTrackMetadata(canonical);
        const QVariant artistId = idByName(db, QStringLiteral("artists"), track.artist);
        const QVariant album = albumId(db, track.album, artistId);
        const QVariant genreId = idByName(db, QStringLiteral("genres"), track.genre);
        const QString coverPath = storeCover(track.coverData, pathKey);
        track.coverData.clear();

        if (knownIt == known.constEnd()) {
            insertTrack.addBindValue(pathKey);
            insertTrack.addBindValue(track.title);
            insertTrack.addBindValue(artistId);
            insertTrack.addBindValue(album);
            insertTrack.addBindValue(genreId);
            insertTrack.addBindValue(track.durationMs);
            insertTrack.addBindValue(coverPath.isEmpty() ? QVariant() : coverPath);
            insertTrack.addBindValue(mtime);
            insertTrack.addBindValue(size);
            insertTrack.addBindValue(QDateTime::currentMSecsSinceEpoch());
            if (!insertTrack.exec()) {
                qWarning() << "MusicLibrary: INSERT трека" << pathKey << ":"
                           << insertTrack.lastError().text();
            }
        } else {
            updateTrack.addBindValue(track.title);
            updateTrack.addBindValue(artistId);
            updateTrack.addBindValue(album);
            updateTrack.addBindValue(genreId);
            updateTrack.addBindValue(track.durationMs);
            updateTrack.addBindValue(coverPath.isEmpty() ? QVariant() : coverPath);
            updateTrack.addBindValue(mtime);
            updateTrack.addBindValue(size);
            updateTrack.addBindValue(knownIt->id);
            if (!updateTrack.exec()) {
                qWarning() << "MusicLibrary: UPDATE трека" << pathKey << ":"
                           << updateTrack.lastError().text();
            }
        }
    }

    // Prune: удаляем из кэша треки, чьих файлов больше нет.
    QList<QPair<qint64, QString>> toDelete;  // id, cover_path
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral("SELECT id, path, cover_path FROM tracks"))) {
            while (query.next()) {
                if (!scannedPaths.contains(query.value(1).toString())) {
                    toDelete.append({query.value(0).toLongLong(),
                                     query.value(2).toString()});
                }
            }
        } else {
            qWarning() << "MusicLibrary: не удалось получить список треков для prune:"
                       << query.lastError().text();
        }
    }

    if (!toDelete.isEmpty()) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral("DELETE FROM tracks WHERE id = ?"));
        const QString coversPrefix = QDir(Database::coversDir()).absolutePath() +
                                     QLatin1Char('/');
        for (const auto& entry : toDelete) {
            remove.addBindValue(entry.first);
            if (!remove.exec()) {
                qWarning() << "MusicLibrary: DELETE трека" << entry.first << ":"
                           << remove.lastError().text();
            }
            if (!entry.second.isEmpty() &&
                QDir::cleanPath(entry.second).startsWith(coversPrefix)) {
                QFile::remove(entry.second);
            }
        }
    }

    if (!db_->commit()) {
        qWarning() << "MusicLibrary: не удалось зафиксировать транзакцию скана";
        db_->rollback();
    }

    loadTracksFromDatabase(db, tracks_);
    emit libraryChanged();
}

const std::vector<Track>& MusicLibrary::tracks() const {
    return tracks_;
}

void MusicLibrary::clear() {
    tracks_.clear();
    emit libraryChanged();
}
