#include "stats/StatsDatabase.hpp"

#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>

namespace {

QStringList tableNames(StatsDatabase& database) {
    QStringList names;
    QSqlQuery query(database.db());
    if (!query.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name"))) {
        return names;
    }
    while (query.next()) names << query.value(0).toString();
    return names;
}

bool insertPlay(StatsDatabase& database, const QString& path, const QString& artist,
                qint64 startedAtMs, qint64 listenedMs) {
    QSqlQuery query(database.db());
    query.prepare(QStringLiteral(
        "INSERT INTO plays(track_path, title, artist, album, started_at_ms, listened_ms,"
        " duration_ms, completed) VALUES (?, ?, ?, '', ?, ?, 180000, 0)"));
    query.addBindValue(path);
    query.addBindValue(QStringLiteral("Title"));
    query.addBindValue(artist);
    query.addBindValue(startedAtMs);
    query.addBindValue(listenedMs);
    return query.exec();
}

}  // namespace

class TestStatsDatabase : public QObject {
    Q_OBJECT

private slots:
    void driverAvailable();
    void createsSchemaOnFirstOpen();
    void reopenPreservesData();
    void migrateIsIdempotent();
    void memoryDatabaseWorks();
    void corruptFileIsRecovered();
    void pathHelpersRoundTrip();
    void emptyPathFails();
};

void TestStatsDatabase::driverAvailable() {
    QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
             "Драйвер QSQLITE не установлен — тесты БД бессмысленны");
}

void TestStatsDatabase::createsSchemaOnFirstOpen() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath(QStringLiteral("stats.db"));
    StatsDatabase database(pathFromDb(filePath));

    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));
    QVERIFY(database.isOpen());
    QCOMPARE(database.schemaVersion(), 0);

    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 1);
    QVERIFY(QFile::exists(filePath));

    const QStringList tables = tableNames(database);
    QVERIFY(tables.contains(QStringLiteral("plays")));
    QVERIFY(tables.contains(QStringLiteral("track_tags")));
    QVERIFY(tables.contains(QStringLiteral("api_cache")));
    QVERIFY(tables.contains(QStringLiteral("lyrics_cache")));
}

void TestStatsDatabase::reopenPreservesData() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("stats.db"));

    {
        StatsDatabase database(pathFromDb(filePath));
        QString error;
        QVERIFY2(database.open(&error), qPrintable(error));
        QVERIFY2(database.migrate(&error), qPrintable(error));
        QVERIFY(insertPlay(database, QStringLiteral("/music/a.mp3"), QStringLiteral("Artist"),
                           1000, 42000));
    }

    {
        StatsDatabase database(pathFromDb(filePath));
        QString error;
        QVERIFY2(database.open(&error), qPrintable(error));
        QCOMPARE(database.schemaVersion(), 1);

        QSqlQuery query(database.db());
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM plays")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }
}

void TestStatsDatabase::migrateIsIdempotent() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    StatsDatabase database(pathFromDb(dir.filePath(QStringLiteral("stats.db"))));
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));
    QVERIFY2(database.migrate(&error), qPrintable(error));

    const QStringList tablesAfterFirst = tableNames(database);
    QCOMPARE(database.schemaVersion(), 1);

    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 1);
    QCOMPARE(tableNames(database), tablesAfterFirst);
}

void TestStatsDatabase::memoryDatabaseWorks() {
    StatsDatabase database(std::filesystem::path(":memory:"));
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));
    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 1);

    QVERIFY(insertPlay(database, QStringLiteral("/music/memory.mp3"),
                       QStringLiteral("Memory"), 2000, 30000));

    QSqlQuery query(database.db());
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM plays")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestStatsDatabase::corruptFileIsRecovered() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("stats.db"));

    {
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("this is definitely not a sqlite database");
    }

    StatsDatabase database(pathFromDb(filePath));
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 1);

    // Повреждённый файл сохранён, а не удалён молча.
    const QStringList corrupt =
        QDir(dir.path()).entryList(QStringList{QStringLiteral("stats.db.corrupt-*")}, QDir::Files);
    QCOMPARE(corrupt.size(), 1);
    QVERIFY(QFile::exists(filePath));

    // Новая БД рабочая.
    QVERIFY(insertPlay(database, QStringLiteral("/music/new.mp3"), QStringLiteral("Artist"),
                       3000, 60000));
    QSqlQuery query(database.db());
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM plays")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestStatsDatabase::pathHelpersRoundTrip() {
    const std::filesystem::path cyrillic =
        std::filesystem::u8path("/tmp/музыка/трек — копия.flac");
    QCOMPARE(pathFromDb(pathToDb(cyrillic)), cyrillic);

    const QString text = QStringLiteral("/home/пользователь/Песни/файл.mp3");
    QCOMPARE(pathToDb(pathFromDb(text)), text);
}

void TestStatsDatabase::emptyPathFails() {
    StatsDatabase database(std::filesystem::path{});
    QString error;
    QVERIFY(!database.open(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!database.isOpen());
}

QTEST_GUILESS_MAIN(TestStatsDatabase)

#include "test_stats_database.moc"
