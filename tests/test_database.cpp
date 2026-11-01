#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/Database.hpp"

// Тесты слоя данных: без QApplication, без GUI (QTEST_GUILESS_MAIN).
class TestDatabase : public QObject {
    Q_OBJECT

private slots:
    void sqliteDriverAvailable();
    void defaultPathIsAbsolute();
    void openCreatesDatabaseFile();
    void migrateCreatesSchemaV1();
    void migrateIsIdempotent();
    void pragmasApplied();
    void transactionRollsBack();

private:
    static QString uniqueConnectionName();
    static QStringList sortedNames(const QString& sql, const QSqlDatabase& db);
};

QString TestDatabase::uniqueConnectionName() {
    static int counter = 0;
    return QStringLiteral("audioplayer-test-%1").arg(++counter);
}

QStringList TestDatabase::sortedNames(const QString& sql, const QSqlDatabase& db) {
    QStringList names;
    QSqlQuery query(db);
    if (!query.exec(sql)) return names;
    while (query.next()) names.append(query.value(0).toString());
    names.sort();
    return names;
}

void TestDatabase::sqliteDriverAvailable() {
    QVERIFY2(QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE")),
             "Драйвер QSQLITE не загружен: проверьте plugins/sqldrivers/qsqlite");
}

void TestDatabase::defaultPathIsAbsolute() {
    const QString path = Database::defaultPath();
    QVERIFY(!path.isEmpty());
    QVERIFY(QFileInfo(path).isAbsolute());
    QVERIFY(path.endsWith(QStringLiteral("/audioplayer.db")));

    const QString covers = Database::coversDir();
    QVERIFY(QFileInfo(covers).isAbsolute());
    QVERIFY(covers.endsWith(QStringLiteral("/covers")));
}

void TestDatabase::openCreatesDatabaseFile() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("library.db"));
    Database db(uniqueConnectionName());

    QVERIFY(db.open(dbPath));
    QVERIFY(db.isOpen());
    QVERIFY(QFile::exists(dbPath));
}

void TestDatabase::migrateCreatesSchemaV1() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Database db(uniqueConnectionName());
    QVERIFY(db.open(tempDir.filePath(QStringLiteral("library.db"))));
    QVERIFY(db.migrate());
    QCOMPARE(db.schemaVersion(), 1);

    QStringList expectedTables = {
        QStringLiteral("albums"),
        QStringLiteral("artists"),
        QStringLiteral("genres"),
        QStringLiteral("lyrics"),
        QStringLiteral("play_events"),
        QStringLiteral("playlist_items"),
        QStringLiteral("playlists"),
        QStringLiteral("tracks"),
    };
    expectedTables.sort();

    const QStringList tables = sortedNames(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' "
                       "AND name NOT LIKE 'sqlite_%'"),
        db.handle());
    QCOMPARE(tables, expectedTables);

    const QStringList indexes = sortedNames(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'index' "
                       "AND name LIKE 'idx_%'"),
        db.handle());
    QCOMPARE(indexes.size(), 6);
    QVERIFY(indexes.contains(QStringLiteral("idx_tracks_genre")));
    QVERIFY(indexes.contains(QStringLiteral("idx_play_events_time")));

    // Схема рабочая: вставляем строку и читаем её обратно.
    QSqlQuery query(db.handle());
    QVERIFY2(query.exec(QStringLiteral("INSERT INTO artists(name) VALUES ('Test Artist')")),
             qPrintable(query.lastError().text()));
    QVERIFY(query.exec(QStringLiteral("SELECT name FROM artists")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Test Artist"));
}

void TestDatabase::migrateIsIdempotent() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("library.db"));
    const QString connection = uniqueConnectionName();

    {
        Database db(connection);
        QVERIFY(db.open(dbPath));
        QVERIFY(db.migrate());
        QVERIFY(db.migrate());
        QCOMPARE(db.schemaVersion(), 1);
    }

    // Повторное открытие уже существующей БД не должно ничего ломать.
    {
        Database db(connection);
        QVERIFY(db.open(dbPath));
        QVERIFY(db.migrate());
        QCOMPARE(db.schemaVersion(), 1);
    }
}

void TestDatabase::pragmasApplied() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Database db(uniqueConnectionName());
    QVERIFY(db.open(tempDir.filePath(QStringLiteral("library.db"))));

    {
        QSqlQuery query(db.handle());
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);

        QVERIFY(query.exec(QStringLiteral("PRAGMA journal_mode")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString().toLower(), QStringLiteral("wal"));
    }

    // Внешние ключи реально включены: вставка с несуществующим artist_id
    // должна упасть.
    QVERIFY(db.migrate());
    QSqlQuery insert(db.handle());
    QVERIFY(!insert.exec(QStringLiteral(
        "INSERT INTO tracks(path, title, artist_id, added_at) "
        "VALUES ('/tmp/x.mp3', 'x', 999999, 0)")));
}

void TestDatabase::transactionRollsBack() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Database db(uniqueConnectionName());
    QVERIFY(db.open(tempDir.filePath(QStringLiteral("library.db"))));
    QVERIFY(db.migrate());

    QVERIFY(db.beginTransaction());
    QSqlQuery query(db.handle());
    QVERIFY(query.exec(QStringLiteral("INSERT INTO genres(name) VALUES ('Rock')")));
    QVERIFY(db.rollback());

    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM genres")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

QTEST_GUILESS_MAIN(TestDatabase)
#include "test_database.moc"
