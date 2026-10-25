#pragma once

#include "gui/SimulatedVisualSource.hpp"

#include <QMainWindow>

class AudioEngine;
class ControlPanel;
class ListeningStats;
class MusicLibrary;
class Playlist;
class Visualizer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AudioEngine& engine, MusicLibrary& library, Playlist& playlist,
               ListeningStats& stats, QWidget* parent = nullptr);

private:
    AudioEngine& engine_;
    MusicLibrary& library_;
    Playlist& playlist_;
    ListeningStats& stats_;
    SimulatedVisualSource visualSource_;
    ControlPanel* control_ = nullptr;
    Visualizer* visualizer_ = nullptr;
};
