#include "library/Playlist.hpp"

#include <QSignalSpy>
#include <QtTest>

namespace {

Track makeTrack(const QString& title) {
    Track track;
    track.title = title;
    track.artist = QStringLiteral("Artist");
    track.album = QStringLiteral("Album");
    track.path = std::filesystem::path("music") / title.toStdString();
    return track;
}

}  // namespace

class PlaylistTest : public QObject {
    Q_OBJECT
private slots:
    void addAppendsAndEmits();
    void insertShiftsCurrentIndex();
    void insertClampsIndex();
    void removeBeforeCurrentShifts();
    void removeCurrentPicksNext();
    void removeLastCurrentFallsBack();
    void moveKeepsCurrentTrack();
    void moveCurrentFollowsTrack();
    void clearResetsCurrent();
    void setCurrentIndexValidates();
    void repeatAndShuffleRoundTrip();
};

void PlaylistTest::addAppendsAndEmits() {
    Playlist playlist;
    QSignalSpy changed(&playlist, &Playlist::changed);

    playlist.add(makeTrack(QStringLiteral("a")));

    QCOMPARE(playlist.size(), 1);
    QCOMPARE(playlist.at(0).title, QStringLiteral("a"));
    QCOMPARE(playlist.currentIndex(), -1);
    QVERIFY(playlist.current() == nullptr);
    QCOMPARE(changed.count(), 1);
}

void PlaylistTest::insertShiftsCurrentIndex() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.add(makeTrack(QStringLiteral("c")));
    playlist.setCurrentIndex(1);

    QSignalSpy changed(&playlist, &Playlist::changed);
    playlist.insert(0, makeTrack(QStringLiteral("x")));

    QCOMPARE(playlist.size(), 4);
    QCOMPARE(playlist.currentIndex(), 2);
    QVERIFY(playlist.current() != nullptr);
    QCOMPARE(playlist.current()->title, QStringLiteral("b"));
    QCOMPARE(playlist.at(0).title, QStringLiteral("x"));
    QCOMPARE(changed.count(), 1);
}

void PlaylistTest::insertClampsIndex() {
    Playlist playlist;

    playlist.insert(-5, makeTrack(QStringLiteral("first")));
    playlist.insert(100, makeTrack(QStringLiteral("last")));

    QCOMPARE(playlist.size(), 2);
    QCOMPARE(playlist.at(0).title, QStringLiteral("first"));
    QCOMPARE(playlist.at(1).title, QStringLiteral("last"));
}

void PlaylistTest::removeBeforeCurrentShifts() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.add(makeTrack(QStringLiteral("c")));
    playlist.setCurrentIndex(2);

    playlist.removeAt(0);

    QCOMPARE(playlist.size(), 2);
    QCOMPARE(playlist.currentIndex(), 1);
    QVERIFY(playlist.current() != nullptr);
    QCOMPARE(playlist.current()->title, QStringLiteral("c"));
}

void PlaylistTest::removeCurrentPicksNext() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.add(makeTrack(QStringLiteral("c")));
    playlist.setCurrentIndex(1);

    QSignalSpy currentChanged(&playlist, &Playlist::currentChanged);
    playlist.removeAt(1);

    QCOMPARE(playlist.size(), 2);
    QCOMPARE(playlist.currentIndex(), 1);
    QVERIFY(playlist.current() != nullptr);
    QCOMPARE(playlist.current()->title, QStringLiteral("c"));
    // Индекс не изменился, поэтому currentChanged не должен эмититься.
    QCOMPARE(currentChanged.count(), 0);
}

void PlaylistTest::removeLastCurrentFallsBack() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.setCurrentIndex(1);

    QSignalSpy currentChanged(&playlist, &Playlist::currentChanged);
    playlist.removeAt(1);

    QCOMPARE(playlist.size(), 1);
    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(playlist.current()->title, QStringLiteral("a"));
    QCOMPARE(currentChanged.count(), 1);
    QCOMPARE(currentChanged.at(0).at(0).toInt(), 0);
}

void PlaylistTest::moveKeepsCurrentTrack() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.add(makeTrack(QStringLiteral("c")));
    playlist.add(makeTrack(QStringLiteral("d")));
    playlist.setCurrentIndex(2);

    playlist.move(0, 3);

    QCOMPARE(playlist.size(), 4);
    QCOMPARE(playlist.currentIndex(), 1);
    QCOMPARE(playlist.current()->title, QStringLiteral("c"));
    QCOMPARE(playlist.at(0).title, QStringLiteral("b"));
    QCOMPARE(playlist.at(3).title, QStringLiteral("a"));
}

void PlaylistTest::moveCurrentFollowsTrack() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.add(makeTrack(QStringLiteral("c")));
    playlist.setCurrentIndex(2);

    playlist.move(2, 0);

    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(playlist.current()->title, QStringLiteral("c"));
    QCOMPARE(playlist.at(0).title, QStringLiteral("c"));
}

void PlaylistTest::clearResetsCurrent() {
    Playlist playlist;
    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));
    playlist.setCurrentIndex(0);

    QSignalSpy changed(&playlist, &Playlist::changed);
    QSignalSpy currentChanged(&playlist, &Playlist::currentChanged);
    playlist.clear();

    QCOMPARE(playlist.size(), 0);
    QCOMPARE(playlist.currentIndex(), -1);
    QVERIFY(playlist.current() == nullptr);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(currentChanged.count(), 1);
    QCOMPARE(currentChanged.at(0).at(0).toInt(), -1);
}

void PlaylistTest::setCurrentIndexValidates() {
    Playlist playlist;

    playlist.setCurrentIndex(0);
    QCOMPARE(playlist.currentIndex(), -1);

    playlist.add(makeTrack(QStringLiteral("a")));
    playlist.add(makeTrack(QStringLiteral("b")));

    QSignalSpy currentChanged(&playlist, &Playlist::currentChanged);
    playlist.setCurrentIndex(1);
    QCOMPARE(playlist.currentIndex(), 1);
    QCOMPARE(currentChanged.count(), 1);

    // Повторный тот же индекс и выход за границы не эмитят сигнал.
    playlist.setCurrentIndex(1);
    playlist.setCurrentIndex(2);
    QCOMPARE(playlist.currentIndex(), 1);
    QCOMPARE(currentChanged.count(), 1);

    playlist.setCurrentIndex(-1);
    QCOMPARE(playlist.currentIndex(), -1);
    QVERIFY(playlist.current() == nullptr);
    QCOMPARE(currentChanged.count(), 2);
}

void PlaylistTest::repeatAndShuffleRoundTrip() {
    Playlist playlist;

    QCOMPARE(static_cast<int>(playlist.repeat()),
             static_cast<int>(Playlist::Repeat::Off));
    QCOMPARE(playlist.shuffle(), false);

    playlist.setRepeat(Playlist::Repeat::Track);
    playlist.setShuffle(true);
    QCOMPARE(static_cast<int>(playlist.repeat()),
             static_cast<int>(Playlist::Repeat::Track));
    QCOMPARE(playlist.shuffle(), true);

    playlist.setRepeat(Playlist::Repeat::Playlist);
    QCOMPARE(static_cast<int>(playlist.repeat()),
             static_cast<int>(Playlist::Repeat::Playlist));
}

QTEST_GUILESS_MAIN(PlaylistTest)
#include "test_playlist.moc"
