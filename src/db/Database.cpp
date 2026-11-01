#include "db/Database.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QDebug>

#include <utility>

namespace {

// Актуальная версия схемы. Новая миграция = +1 и ветка в migrate().
constexpr int kSchemaVersion = 1;

QStringList schemaV1Statements() {
    return {
        QStringLiteral("CREATE TABLE artists ("
                       " id   INTEGER PRIMARY KEY,"
                       " name TEXT NOT NULL UNIQUE)"),
        QStringLiteral("CREATE TABLE albums ("
                       " id        INTEGER PRIMARY KEY,"
                       " title     TEXT NOT NULL,"
                       " artist_id INTEGER REFERENCES artists(id),"
                       " UNIQUE(title, artist_id))"),
        QStringLiteral("CREATE TABLE genres ("
                       " id   INTEGER PRIMARY KEY,"
                       " name TEXT NOT NULL UNIQUE)"),
        QStringLiteral("CREATE TABLE tracks ("
                       " id          INTEGER PRIMARY KEY,"
                       " path        TEXT NOT NULL UNIQUE,"
                       " title       TEXT NOT NULL,"
                       " artist_id   INTEGER REFERENCES artists(id),"
                       " album_id    INTEGER REFERENCES albums(id),"
                       " genre_id    INTEGER REFERENCES genres(id),"
                       " duration_ms INTEGER NOT NULL DEFAULT 0,"
                       " cover_path  TEXT,"
                       " file_mtime  INTEGER NOT NULL DEFAULT 0,"
                       " file_size   INTEGER NOT NULL DEFAULT 0,"
                       " added_at    INTEGER NOT NULL)"),
        // Задел на будущее: в этом этапе плейлисты живут в .plst.
        QStringLiteral("CREATE TABLE playlists ("
                       " id         INTEGER PRIMARY KEY,"
                       " name       TEXT NOT NULL,"
                       " created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE playlist_items ("
                       " playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,"
                       " track_id    INTEGER NOT NULL REFERENCES tracks(id)    ON DELETE CASCADE,"
                       " position    INTEGER NOT NULL,"
                       " PRIMARY KEY (playlist_id, position))"),
        QStringLiteral("CREATE TABLE play_events ("
                       " id              INTEGER PRIMARY KEY,"
                       " track_id        INTEGER REFERENCES tracks(id) ON DELETE SET NULL,"
                       " path_snapshot   TEXT NOT NULL,"
                       " artist_snapshot TEXT NOT NULL,"
                       " played_at       INTEGER NOT NULL,"
                       " listened_ms     INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE lyrics ("
                       " id         INTEGER PRIMARY KEY,"
                       " track_key  TEXT NOT NULL UNIQUE,"
                       " source     TEXT NOT NULL,"
                       " is_synced  INTEGER NOT NULL DEFAULT 0,"
                       " content    TEXT NOT NULL,"
                       " fetched_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX idx_play_events_track  ON play_events(track_id)"),
        QStringLiteral("CREATE INDEX idx_play_events_time   ON play_events(played_at)"),
        QStringLiteral("CREATE INDEX idx_play_events_artist ON play_events(artist_snapshot)"),
        QStringLiteral("CREATE INDEX idx_tracks_artist      ON tracks(artist_id)"),
        QStringLiteral("CREATE INDEX idx_tracks_album       ON tracks(album_id)"),
        QStringLiteral("CREATE INDEX idx_tracks_genre       ON tracks(genre_id)"),
    };
}

bool execAll(const QSqlDatabase& db, const QStringList& statements, QString* error) {
    for (const QString& sql : statements) {
        QSqlQuery query(db);
        if (!query.exec(sql)) {
            if (error != nullptr) *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

}  // namespace

Database::Database(QString connectionName) : connectionName_(std::move(connectionName)) {}

Database::~Database() {
    if (!QSqlDatabase::contains(connectionName_)) return;

    // Важно: локальный QSqlDatabase должен быть уничтожен до removeDatabase,
    // иначе Qt печатает «connection is still in use».
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool Database::open(const QString& filePath) {
    if (filePath.isEmpty()) {
        qWarning() << "Database::open: пустой путь к файлу БД";
        return false;
    }

    const QFileInfo info(filePath);
    const QString dir = info.absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "Database::open: не удалось создать каталог" << dir;
        return false;
    }

    // Повторный open() на том же объекте: закрываем прежнее соединение.
    if (QSqlDatabase::contains(connectionName_)) {
        {
            QSqlDatabase existing = QSqlDatabase::database(connectionName_, false);
            if (existing.isOpen()) existing.close();
        }
        QSqlDatabase::removeDatabase(connectionName_);
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
        db.setDatabaseName(filePath);
        if (!db.open()) {
            qWarning() << "Database::open:" << db.lastError().text();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName_);
            return false;
        }
    }

    return applyPragmas();
}

bool Database::isOpen() const {
    return QSqlDatabase::contains(connectionName_) && handle().isOpen();
}

bool Database::applyPragmas() {
    const QSqlDatabase db = handle();

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        qWarning() << "Database: PRAGMA foreign_keys:" << query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        qWarning() << "Database: PRAGMA synchronous:" << query.lastError().text();
        return false;
    }

    // WAL может быть недоступен на сетевых/экзотических ФС — это не повод
    // не открывать БД (см. раздел 12 «Риски»).
    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        qWarning() << "Database: PRAGMA journal_mode = WAL:" << query.lastError().text();
    } else if (query.next()) {
        const QString mode = query.value(0).toString();
        if (mode.compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
            qWarning() << "Database: WAL недоступен, режим журнала:" << mode;
        }
    }
    return true;
}

bool Database::migrate() {
    if (!isOpen()) return false;

    int version = schemaVersion();
    if (version < 0) return false;
    if (version > kSchemaVersion) {
        qWarning() << "Database: версия схемы" << version
                   << "новее поддерживаемой" << kSchemaVersion;
        return false;
    }

    while (version < kSchemaVersion) {
        if (!beginTransaction()) return false;

        bool ok = false;
        QString error;
        switch (version) {
        case 0:
            ok = migrateToV1();
            break;
        default:
            error = QStringLiteral("нет миграции с версии %1").arg(version);
            break;
        }

        if (ok) {
            QSqlQuery query(handle());
            ok = query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version + 1));
            if (!ok) error = query.lastError().text();
        }

        if (!ok) {
            rollback();
            qWarning() << "Database: миграция" << version << "->" << (version + 1)
                       << "не удалась:" << error;
            return false;
        }
        if (!commit()) {
            rollback();
            return false;
        }
        ++version;
    }

    return true;
}

bool Database::migrateToV1() {
    QString error;
    if (!execAll(handle(), schemaV1Statements(), &error)) {
        qWarning() << "Database: схема v1:" << error;
        return false;
    }
    return true;
}

QSqlDatabase Database::handle() const {
    if (!QSqlDatabase::contains(connectionName_)) return QSqlDatabase();
    return QSqlDatabase::database(connectionName_, false);
}

bool Database::beginTransaction() {
    if (!isOpen()) return false;
    return handle().transaction();
}

bool Database::commit() {
    if (!isOpen()) return false;
    return handle().commit();
}

bool Database::rollback() {
    if (!isOpen()) return false;
    return handle().rollback();
}

int Database::schemaVersion() const {
    if (!isOpen()) return -1;

    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) return -1;
    return query.value(0).toInt();
}

QString Database::defaultPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("audioplayer.db"));
}

QString Database::coversDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("covers"));
}
