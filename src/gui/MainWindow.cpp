#include "gui/MainWindow.hpp"

#include "audio/AudioEngine.hpp"
#include "gui/ControlPanel.hpp"
#include "gui/LibraryPanel.hpp"
#include "gui/Visualizer.hpp"
#include "library/MusicLibrary.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"
#include "stats/ListeningStats.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
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

    layout->addWidget(visualizer_, 1);
    layout->addWidget(control_);
    setCentralWidget(central);

    connect(&playlist_, &Playlist::currentChanged, this, [this](int index) {
        if (index < 0 || index >= playlist_.size()) return;
        engine_.open(playlist_.at(index).path);
    });

    connect(control_, &ControlPanel::nextEffectRequested, visualizer_,
            &Visualizer::cycleMode);
    connect(visualizer_, &Visualizer::modeChanged, control_, [this] {
        control_->setEffectName(visualizer_->modeName());
    });
    control_->setEffectName(visualizer_->modeName());

    // Плейлист подхватывает содержимое библиотеки, пока пуст
    connect(&library_, &MusicLibrary::libraryChanged, this, [this] {
        if (playlist_.size() > 0) return;
        for (const Track& track : library_.tracks()) playlist_.add(track);
        if (playlist_.size() > 0) playlist_.setCurrentIndex(0);
    });

    // Боковая панель «Библиотека» (выбор папки + список треков)
    auto* libraryPanel = new LibraryPanel(library_, this);
    auto* dock = new QDockWidget(tr("Библиотека"), this);
    dock->setObjectName(QStringLiteral("libraryDock"));
    dock->setMinimumWidth(560);
    dock->setWidget(libraryPanel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    auto* viewMenu = menuBar()->addMenu(tr("Вид"));
    viewMenu->addAction(dock->toggleViewAction());

    connect(libraryPanel, &LibraryPanel::trackActivated, this, [this](int index) {
        const auto& tracks = library_.tracks();
        if (index < 0 || index >= static_cast<int>(tracks.size())) return;
        const Track& track = tracks[static_cast<std::size_t>(index)];

        int playlistIndex = -1;
        for (int i = 0; i < playlist_.size(); ++i) {
            if (playlist_.at(i).path == track.path) {
                playlistIndex = i;
                break;
            }
        }
        if (playlistIndex == -1) {
            playlist_.add(track);
            playlistIndex = playlist_.size() - 1;
        }
        playlist_.setCurrentIndex(playlistIndex);
        engine_.play();
    });

    if (const Track* track = playlist_.current()) engine_.open(track->path);

    setWindowTitle(QStringLiteral("LOCAL AUDIO PLAYER"));
    resize(1060, 580);
}
