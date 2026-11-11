#include "library/MusicLibrary.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

std::filesystem::path toPath(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toStdString());
#endif
}

bool writeEmptyFile(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

}  // namespace

class MusicLibraryTest : public QObject {
    Q_OBJECT
private slots:
    void scanFindsAudioFilesRecursively();
    void scanIsIdempotent();
    void scanMissingDirectoryStaysEmpty();
    void clearEmptiesTracks();
};

void MusicLibraryTest::scanFindsAudioFilesRecursively() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVERIFY(writeEmptyFile(dir.filePath(QStringLiteral("b.mp3"))));
    QVERIFY(writeEmptyFile(dir.filePath(QStringLiteral("nested/a.flac"))));
    QVERIFY(writeEmptyFile(dir.filePath(QStringLiteral("notes.txt"))));
    QVERIFY(writeEmptyFile(dir.filePath(QStringLiteral("cover.png"))));

    MusicLibrary library;
    QSignalSpy changed(&library, &MusicLibrary::libraryChanged);

    library.scan(toPath(dir.path()));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(library.tracks().size(), 2);

    // Теги в пустых файлах отсутствуют, поэтому заголовок берётся из имени
    // файла, а список сортируется по нему.
    QCOMPARE(library.tracks()[0].title, QStringLiteral("a"));
    QCOMPARE(library.tracks()[1].title, QStringLiteral("b"));
    QVERIFY(library.tracks()[0].durationMs > 0);
    QVERIFY(library.tracks()[1].durationMs > 0);
}

void MusicLibraryTest::scanIsIdempotent() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeEmptyFile(dir.filePath(QStringLiteral("track.wav"))));

    MusicLibrary library;
    QSignalSpy changed(&library, &MusicLibrary::libraryChanged);

    library.scan(toPath(dir.path()));
    library.scan(toPath(dir.path()));

    // Повторный скан не должен накапливать дубликаты.
    QCOMPARE(library.tracks().size(), 1);
    QCOMPARE(changed.count(), 2);
}

void MusicLibraryTest::scanMissingDirectoryStaysEmpty() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MusicLibrary library;
    QSignalSpy changed(&library, &MusicLibrary::libraryChanged);

    library.scan(toPath(dir.filePath(QStringLiteral("does-not-exist"))));

    QCOMPARE(library.tracks().size(), 0);
    QCOMPARE(changed.count(), 1);
}

void MusicLibraryTest::clearEmptiesTracks() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeEmptyFile(dir.filePath(QStringLiteral("a.ogg"))));

    MusicLibrary library;
    library.scan(toPath(dir.path()));
    QCOMPARE(library.tracks().size(), 1);

    QSignalSpy changed(&library, &MusicLibrary::libraryChanged);
    library.clear();

    QCOMPARE(library.tracks().size(), 0);
    QCOMPARE(changed.count(), 1);
}

QTEST_GUILESS_MAIN(MusicLibraryTest)
#include "test_musiclibrary.moc"
