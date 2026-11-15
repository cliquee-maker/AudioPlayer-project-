#include "stats/StatsDatabase.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QDebug>

#include <functional>
#include <map>
#include <utility>

namespace {

// Актуальная версия схемы статистики. Новая миграция = +1 и ветка в migrations().
constexpr int kSchemaVersion = 1;

QStringList schemaV1Statements() {
    return {
        // События прослушивания. artist/album/title хранятся снимком на момент
        // прослушивания: статистика не ломается при переименовании тегов/файла.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS plays ("
            " id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            " track_path    TEXT    NOT NULL,"
            " title         TEXT    NOT NULL DEFAULT '',"
            " artist        TEXT    NOT NULL DEFAULT '',"
            " album         TEXT    NOT NULL DEFAULT '',"
            " started_at_ms INTEGER NOT NULL,"
            " listened_ms   INTEGER NOT NULL CHECK (listened_ms >= 0),"
            " duration_ms   INTEGER NOT NULL DEFAULT 0,"
            " completed     INTEGER NOT NULL DEFAULT 0 CHECK (completed IN (0, 1)))"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_plays_started"
            " ON plays(started_at_ms)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_plays_path_started"
            " ON plays(track_path, started_at_ms)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_plays_artist_started"
            " ON plays(artist, started_at_ms)"),

        // Теги/жанры из TagLib или MusicBrainz. Ключ — путь к файлу, источник
        // хранится отдельно, чтобы локальные теги не перезаписывались.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS track_tags ("
            " track_path TEXT NOT NULL,"
            " tag        TEXT NOT NULL,"
            " source     TEXT NOT NULL DEFAULT 'taglib',"
            " PRIMARY KEY (track_path, tag, source))"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_track_tags_tag"
            " ON track_tags(tag)"),

        // Кэш произвольных JSON-ответов API.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS api_cache ("
            " cache_key     TEXT PRIMARY KEY,"
            " provider      TEXT NOT NULL,"
            " payload       TEXT NOT NULL,"
            " fetched_at_ms INTEGER NOT NULL,"
            " expires_at_ms INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_api_cache_expires"
            " ON api_cache(expires_at_ms)"),

        // Кэш текстов песен.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS lyrics_cache ("
            " cache_key     TEXT PRIMARY KEY,"
            " artist        TEXT NOT NULL,"
            " title         TEXT NOT NULL,"
            " album         TEXT NOT NULL DEFAULT '',"
            " duration_s    INTEGER NOT NULL DEFAULT 0,"
            " provider      TEXT NOT NULL,"
            " status        TEXT NOT NULL,"
            " plain         TEXT,"
            " synced        TEXT,"
            " provider_id   INTEGER,"
            " fetched_at_ms INTEGER NOT NULL,"
            " expires_at_ms INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_lyrics_cache_expires"
            " ON lyrics_cache(expires_at_ms)"),
    };
}

bool execAll(QSqlDatabase& database, const QStringList& statements, QString* error) {
    for (const QString& sql : statements) {
        QSqlQuery query(database);
        if (!query.exec(sql)) {
            if (error != nullptr) *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool migrateToV1(QSqlDatabase& database, QString* error) {
    return execAll(database, schemaV1Statements(), error);
}

using Migration = std::function<bool(QSqlDatabase&, QString*)>;

const std::map<int, Migration>& migrations() {
    static const std::map<int, Migration> table = {
        {0, &migrateToV1},
    };
    return table;
}

// Уникальный суффикс для .bak-* / .corrupt-*: миллисекунды, чтобы несколько
// попыток в одну секунду не перетирали друг друга.
QString timestampSuffix() {
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
}

}  // namespace

QString pathToDb(const std::filesystem::path& path) {
    // u8string() даёт UTF-8 независимо от локали (важно для кириллицы на Windows).
    const std::string utf8 = path.u8string();
    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

std::filesystem::path pathFromDb(const QString& value) {
    const QByteArray utf8 = value.toUtf8();
    return std::filesystem::u8path(utf8.constData());
}

StatsDatabase::StatsDatabase(std::filesystem::path file)
    : connectionName_(QStringLiteral("stats-%1").arg(
          static_cast<qulonglong>(reinterpret_cast<quintptr>(this)), 0, 16)),
      file_(std::move(file)) {}

StatsDatabase::~StatsDatabase() {
    close();
}

bool StatsDatabase::open(QString* error) {
    if (error != nullptr) error->clear();

    if (file_.empty()) {
        if (error != nullptr) *error = QStringLiteral("путь к файлу БД не задан");
        return false;
    }

    // Спецзначение ":memory:" не должно приводить к созданию каталогов.
    if (!isMemory()) {
        const QString dir = QFileInfo(dbFileName()).absolutePath();
        if (!dir.isEmpty() && !QDir().mkpath(dir)) {
            if (error != nullptr) {
                *error = QStringLiteral("не удалось создать каталог %1").arg(dir);
            }
            return false;
        }
    }

    QString localError;
    QString* err = error != nullptr ? error : &localError;

    if (!openConnection(err)) {
        if (!isCorruption(*err)) return false;
        return recoverFromCorruption(err);
    }

    if (!applyPragmas(err)) {
        close();
        if (!isCorruption(*err)) return false;
        return recoverFromCorruption(err);
    }

    if (!probe(err)) {
        close();
        if (!isCorruption(*err)) return false;
        return recoverFromCorruption(err);
    }

    return true;
}

bool StatsDatabase::migrate(QString* error) {
    if (error != nullptr) error->clear();

    if (!isOpen()) {
        if (error != nullptr) *error = QStringLiteral("БД не открыта");
        return false;
    }

    int version = schemaVersion();
    if (version < 0) {
        if (error != nullptr) *error = QStringLiteral("не удалось прочитать PRAGMA user_version");
        return false;
    }
    if (version > kSchemaVersion) {
        if (error != nullptr) {
            *error = QStringLiteral("версия схемы %1 новее поддерживаемой %2")
                         .arg(version)
                         .arg(kSchemaVersion);
        }
        return false;
    }

    while (version < kSchemaVersion) {
        if (!backupBeforeMigration(error)) return false;

        QSqlDatabase database = db();
        if (!database.transaction()) {
            if (error != nullptr) *error = database.lastError().text();
            return false;
        }

        QString stepError;
        bool ok = false;
        const auto& table = migrations();
        const auto it = table.find(version);
        if (it == table.end()) {
            stepError = QStringLiteral("нет миграции с версии %1").arg(version);
        } else {
            ok = it->second(database, &stepError);
        }

        if (ok) {
            // PRAGMA не принимает плейсхолдеры, но значение — наше целое число.
            QSqlQuery query(database);
            ok = query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version + 1));
            if (!ok) stepError = query.lastError().text();
        }

        if (!ok) {
            database.rollback();
            // Битая БД может проявиться только на этапе миграции: сохраняем файл
            // и повторяем миграцию на чистой БД.
            if (isCorruption(stepError) && !recovering_ && recoverFromCorruption(error)) {
                return migrate(error);
            }
            if (error != nullptr) *error = stepError;
            return false;
        }

        if (!database.commit()) {
            const QString commitError = database.lastError().text();
            database.rollback();
            if (error != nullptr) *error = commitError;
            return false;
        }

        ++version;
    }

    return true;
}

bool StatsDatabase::isOpen() const {
    return QSqlDatabase::contains(connectionName_) && db().isOpen();
}

void StatsDatabase::close() {
    if (!QSqlDatabase::contains(connectionName_)) return;

    // Локальный QSqlDatabase должен быть уничтожен до removeDatabase,
    // иначе Qt печатает «connection ... is still in use».
    {
        QSqlDatabase database = QSqlDatabase::database(connectionName_, false);
        if (database.isOpen()) database.close();
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

QSqlDatabase StatsDatabase::db() const {
    if (!QSqlDatabase::contains(connectionName_)) return QSqlDatabase();
    return QSqlDatabase::database(connectionName_, false);
}

int StatsDatabase::schemaVersion() const {
    if (!isOpen()) return -1;

    QSqlQuery query(db());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) return -1;
    return query.value(0).toInt();
}

const std::filesystem::path& StatsDatabase::file() const {
    return file_;
}

bool StatsDatabase::openConnection(QString* error) {
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (error != nullptr) *error = QStringLiteral("драйвер QSQLITE недоступен");
        return false;
    }

    if (QSqlDatabase::contains(connectionName_)) close();

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          connectionName_);
        database.setDatabaseName(dbFileName());
        if (!database.open()) {
            if (error != nullptr) *error = database.lastError().text();
            database = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName_);
            return false;
        }
    }

    return true;
}

bool StatsDatabase::applyPragmas(QString* error) {
    const QSqlDatabase database = db();
    if (!database.isValid()) {
        if (error != nullptr) *error = QStringLiteral("соединение с БД не создано");
        return false;
    }

    auto execRequired = [&](const QString& sql) -> bool {
        QSqlQuery query(database);
        if (query.exec(sql)) return true;
        if (error != nullptr) *error = query.lastError().text();
        return false;
    };

    if (!execRequired(QStringLiteral("PRAGMA foreign_keys = ON"))) return false;
    if (!execRequired(QStringLiteral("PRAGMA synchronous = NORMAL"))) return false;
    if (!execRequired(QStringLiteral("PRAGMA busy_timeout = 5000"))) return false;

    // WAL даёт устойчивость к падению и не блокирует чтение записью. На сетевых
    // и экзотических ФС он может быть недоступен — это не повод не открывать БД.
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        const QString text = query.lastError().text();
        if (isCorruption(text)) {
            if (error != nullptr) *error = text;
            return false;
        }
        qWarning() << "StatsDatabase: PRAGMA journal_mode = WAL:" << text;
    } else if (query.next()) {
        const QString mode = query.value(0).toString();
        if (!isMemory() && mode.compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
            qWarning() << "StatsDatabase: WAL недоступен, режим журнала:" << mode;
        }
    }

    return true;
}

bool StatsDatabase::probe(QString* error) {
    QSqlQuery query(db());
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master")) && query.next()) {
        return true;
    }
    if (error != nullptr) *error = query.lastError().text();
    return false;
}

bool StatsDatabase::backupBeforeMigration(QString* error) {
    if (isMemory()) return true;

    // Новый файл (версия 0) копировать незачем.
    if (schemaVersion() <= 0) return true;

    const QString path = dbFileName();
    if (!QFile::exists(path)) return true;

    // Сбрасываем WAL в основной файл, иначе копия может быть неполной.
    {
        QSqlQuery checkpoint(db());
        checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"));
    }

    QString backup = path + QStringLiteral(".bak-") + timestampSuffix();
    int suffix = 1;
    while (QFile::exists(backup)) {
        backup = path + QStringLiteral(".bak-") + timestampSuffix()
                 + QStringLiteral("-%1").arg(suffix++);
    }

    if (!QFile::copy(path, backup)) {
        if (error != nullptr) {
            *error = QStringLiteral("не удалось создать копию БД %1").arg(backup);
        }
        return false;
    }
    return true;
}

bool StatsDatabase::recoverFromCorruption(QString* error) {
    if (recovering_) {
        if (error != nullptr) *error = QStringLiteral("повторное обнаружение повреждения БД");
        return false;
    }

    recovering_ = true;
    struct Guard {
        bool& flag;
        ~Guard() { flag = false; }
    } guard{recovering_};

    const QString reason = error != nullptr ? *error : QString();
    close();

    if (!isMemory()) {
        const QString path = dbFileName();
        QString corruptPath = path + QStringLiteral(".corrupt-") + timestampSuffix();
        int suffix = 1;
        while (QFile::exists(corruptPath)) {
            corruptPath = path + QStringLiteral(".corrupt-") + timestampSuffix()
                          + QStringLiteral("-%1").arg(suffix++);
        }

        if (QFile::exists(path) && !QFile::rename(path, corruptPath)) {
            if (error != nullptr) {
                *error = QStringLiteral("не удалось сохранить повреждённую БД %1").arg(path);
            }
            return false;
        }
        // Побочные файлы WAL/SHM относятся к повреждённой БД.
        QFile::remove(path + QStringLiteral("-wal"));
        QFile::remove(path + QStringLiteral("-shm"));

        qWarning() << "StatsDatabase: повреждённая БД сохранена как" << corruptPath
                   << ", создаётся новая. Причина:" << reason;
    } else {
        qWarning() << "StatsDatabase: повреждена in-memory БД, создаётся новая";
    }

    if (!openConnection(error) || !applyPragmas(error) || !probe(error)) return false;
    return migrate(error);
}

bool StatsDatabase::isCorruption(const QString& errorText) const {
    const QString text = errorText.toLower();
    return text.contains(QStringLiteral("malformed"))
        || text.contains(QStringLiteral("not a database"))
        || text.contains(QStringLiteral("file is encrypted"))
        || text.contains(QStringLiteral("disk image"));
}

bool StatsDatabase::isMemory() const {
    return file_ == std::filesystem::path(":memory:");
}

QString StatsDatabase::dbFileName() const {
    if (isMemory()) return QStringLiteral(":memory:");
    return pathToDb(file_);
}
