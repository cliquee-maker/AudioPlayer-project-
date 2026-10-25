#include <QApplication>

#include "audio/AudioEngine.hpp"
#include "gui/MainWindow.hpp"
#include "library/MusicLibrary.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"
#include "stats/ListeningStats.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("AudioPlayer"));
    app.setApplicationName(QStringLiteral("LOCAL AUDIO PLAYER"));

    AudioEngine engine;
    MusicLibrary library;
    Playlist playlist;
    ListeningStats stats;

    library.scan(AUDIOPLAYER_ASSETS_DIR "/music");
    for (const Track& track : library.tracks()) playlist.add(track);
    if (playlist.size() > 0) playlist.setCurrentIndex(0);

    MainWindow window(engine, library, playlist, stats);
    window.show();

    return app.exec();
}
