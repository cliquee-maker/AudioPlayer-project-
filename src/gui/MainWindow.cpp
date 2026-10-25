#include "gui/MainWindow.hpp"

#include "audio/AudioEngine.hpp"
#include "gui/ControlPanel.hpp"
#include "gui/Visualizer.hpp"
#include "library/MusicLibrary.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"
#include "stats/ListeningStats.hpp"

#include <QVBoxLayout>

MainWindow::MainWindow(AudioEngine& engine, MusicLibrary& library, Playlist& playlist,
                       ListeningStats& stats, QWidget* parent)
    : QMainWindow(parent),
      engine_(engine),
      library_(library),
      playlist_(playlist),
      stats_(stats),
      visualSource_(engine) {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    control_ = new ControlPanel(engine_, playlist_, central);
    visualizer_ = new Visualizer(&visualSource_, central);

    layout->addWidget(control_);
    layout->addWidget(visualizer_, 1);
    setCentralWidget(central);

    connect(&playlist_, &Playlist::currentChanged, this, [this](int index) {
        if (index < 0 || index >= playlist_.size()) return;
        engine_.open(playlist_.at(index).path);
    });

    if (const Track* track = playlist_.current()) engine_.open(track->path);

    setWindowTitle(QStringLiteral("LOCAL AUDIO PLAYER"));
    resize(860, 540);
}
