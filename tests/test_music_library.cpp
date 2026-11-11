#include <QtTest/QtTest>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <attachedpictureframe.h>
#include <id3v2tag.h>
#include <tag.h>
#include <wavfile.h>

#include "db/Database.hpp"
#include "library/MusicLibrary.hpp"

// Тесты этапа 2: инкрементальный скан библиотеки, теги/жанр, кэш обложек.
class TestMusicLibrary : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void incrementalScanSkipsUnchangedFiles();
    void readsTagsGenreAndCachesCover();

private:
    static QString uniqueConnectionName();
    static bool writeSilentWav(const QString& filePath);
    static bool writeTags(const QString& filePath);
};

void TestMusicLibrary::initTestCase() {
    // Не трогаем реальный AppDataLocation пользователя при тестах кэша обложек.
    QStandardPaths::setTestModeEnabled(true);
}

QString TestMusicLibrary::uniqueConnectionName() {
    static int counter = 0;
    return QStringLiteral("audioplayer-library-test-%1").arg(++counter);
}

bool TestMusicLibrary::writeSilentWav(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    const quint16 channels = 1;
    const quint32 sampleRate = 8000;
    const quint16 bitsPerSample = 16;
    const quint32 dataSize = sampleRate * channels * bitsPerSample / 8;  // 1 секунда

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataSize);
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << quint32(16) << quint16(1) << channels << sampleRate
        << quint32(sampleRate * channels * bitsPerSample / 8)
        << quint16(channels * bitsPerSample / 8) << bitsPerSample;
    out.writeRawData("data", 4);
    out << dataSize;

    const QByteArray silence(static_cast<int>(dataSize), '\0');
    return out.writeRawData(silence.constData(), silence.size()) == silence.size();
}

bool TestMusicLibrary::writeTags(const QString& filePath) {
    TagLib::RIFF::WAV::File wav(filePath.toUtf8().constData());
    if (!wav.isValid() || wav.tag() == nullptr) return false;

    wav.tag()->setTitle("Tagged Track");
    wav.tag()->setArtist("Test Artist");
    wav.tag()->setAlbum("Test Album");
    wav.tag()->setGenre("Jazz");

    auto* id3 = wav.ID3v2Tag();
    if (id3 == nullptr) return false;

    // Обложка: содержимое не обязано быть валидным PNG для TagLib, но
    // MusicLibrary определяет расширение по сигнатуре — сигнатуру даём.
    QByteArray png = QByteArray::fromHex("89504e470d0a1a0a0000000d49484452");
    png.append(QByteArrayLiteral("not-a-real-png-payload"));

    auto* picture = new TagLib::ID3v2::AttachedPictureFrame;
    picture->setMimeType("image/png");
    picture->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    picture->setPicture(TagLib::ByteVector(png.constData(), png.size()));
    id3->addFrame(picture);

    return wav.save();
}

void TestMusicLibrary::incrementalScanSkipsUnchangedFiles() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString musicDir = tempDir.filePath(QStringLiteral("music"));
    QVERIFY(QDir().mkpath(musicDir + QStringLiteral("/nested")));

    const QString alpha = musicDir + QStringLiteral("/Alpha.mp3");
    const QString beta = musicDir + QStringLiteral("/nested/Beta.flac");
    QVERIFY(QFile(alpha).open(QIODevice::WriteOnly));
    QVERIFY(QFile(beta).open(QIODevice::WriteOnly));

    Database db(uniqueConnectionName());
    QVERIFY(db.open(tempDir.filePath(QStringLiteral("library.db"))));
    QVERIFY(db.migrate());

    MusicLibrary library(db);
    QSignalSpy changed(&library, &MusicLibrary::libraryChanged);

    library.scan(musicDir.toStdString());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(library.tracks().size(), std::size_t(2));
    // Треки отсортированы по пути: Alpha раньше nested/Beta.
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Alpha"));
    QCOMPARE(library.tracks().at(1).title, QStringLiteral("Beta"));

    QSqlQuery count(db.handle());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM tracks")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 2);

    // Подменяем title прямо в кэше. Если второй скан пропустит неизменившийся
    // файл (mtime + size совпали), то прочитает именно подменённое значение.
    QSqlQuery tamper(db.handle());
    tamper.prepare(QStringLiteral("UPDATE tracks SET title = 'Cached Title' WHERE path = ?"));
    tamper.addBindValue(QString::fromStdString(alpha.toStdString()));
    QVERIFY2(tamper.exec(), qPrintable(tamper.lastError().text()));

    library.scan(musicDir.toStdString());
    QCOMPARE(changed.count(), 2);
    QCOMPARE(library.tracks().size(), std::size_t(2));
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Cached Title"));

    // Меняем mtime -> скан обязан перечитать теги и вернуть fallback-имя.
    QFile alphaFile(alpha);
    QVERIFY(alphaFile.open(QIODevice::ReadOnly));
    QVERIFY(alphaFile.setFileTime(QDateTime::currentDateTime().addSecs(60),
                                  QFileDevice::FileModificationTime));
    alphaFile.close();

    library.scan(musicDir.toStdString());
    QCOMPARE(changed.count(), 3);
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Alpha"));

    // Удалённый файл вычищается из кэша (prune).
    QVERIFY(QFile::remove(beta));
    library.scan(musicDir.toStdString());
    QCOMPARE(changed.count(), 4);
    QCOMPARE(library.tracks().size(), std::size_t(1));
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Alpha"));

    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM tracks")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);
}

void TestMusicLibrary::readsTagsGenreAndCachesCover() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString musicDir = tempDir.filePath(QStringLiteral("music"));
    QVERIFY(QDir().mkpath(musicDir));
    const QString wav = musicDir + QStringLiteral("/tagged.wav");
    QVERIFY(writeSilentWav(wav));
    QVERIFY(writeTags(wav));

    // Чистим прежние кэшированные обложки в test-mode AppDataLocation.
    QDir(Database::coversDir()).removeRecursively();

    Database db(uniqueConnectionName());
    QVERIFY(db.open(tempDir.filePath(QStringLiteral("library.db"))));
    QVERIFY(db.migrate());

    MusicLibrary library(db);
    library.scan(musicDir.toStdString());
    QCOMPARE(library.tracks().size(), std::size_t(1));

    const Track& track = library.tracks().front();
    QCOMPARE(track.title, QStringLiteral("Tagged Track"));
    QCOMPARE(track.artist, QStringLiteral("Test Artist"));
    QCOMPARE(track.album, QStringLiteral("Test Album"));
    QCOMPARE(track.genre, QStringLiteral("Jazz"));
    QVERIFY(track.durationMs > 900 && track.durationMs < 1100);
    QVERIFY(!track.coverData.isEmpty());

    QSqlQuery query(db.handle());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT t.cover_path, g.name "
        "FROM tracks t JOIN genres g ON g.id = t.genre_id")));
    QVERIFY(query.next());

    const QString coverPath = query.value(0).toString();
    QVERIFY(!coverPath.isEmpty());
    QVERIFY(QFileInfo(coverPath).isAbsolute());
    QVERIFY(QFileInfo::exists(coverPath));
    QVERIFY(QDir::cleanPath(coverPath).startsWith(
        QDir(Database::coversDir()).absolutePath()));
    QCOMPARE(query.value(1).toString(), QStringLiteral("Jazz"));

    // Вторая строка в genres при повторном скане не появляется.
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM genres WHERE name = 'Jazz'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

QTEST_GUILESS_MAIN(TestMusicLibrary)
#include "test_music_library.moc"
