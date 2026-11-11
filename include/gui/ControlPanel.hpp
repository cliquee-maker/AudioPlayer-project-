#pragma once

#include <QWidget>

class AudioEngine;
class Playlist;
class QLabel;
class QSlider;
class QToolButton;
class VolumeDial;

class ControlPanel : public QWidget {
    Q_OBJECT
public:
    ControlPanel(AudioEngine& engine, Playlist& playlist, QWidget* parent = nullptr,
                 QWidget* visualizer = nullptr);

public slots:
    void next();
    void prev();
    void handleMediaFinished();
    void setEffectName(const QString& name);
    void setMuted(bool muted);

signals:
    void nextEffectRequested();

private slots:
    void onStateChanged(bool playing);
    void onPositionChanged(qint64 ms);
    void onDurationChanged(qint64 ms);
    void onSeekMoved(int ms);
    void onSeekReleased();
    void onRepeatClicked();

private:
    void refreshRepeatButton();
    void refreshTimeLabel(qint64 shownMs);

    AudioEngine& engine_;
    Playlist& playlist_;

    QToolButton* prevBtn_;
    QToolButton* playBtn_;
    QToolButton* stopBtn_;
    QToolButton* nextBtn_;
    QToolButton* repeatBtn_;
    QToolButton* effectBtn_;
    QToolButton* shuffleBtn_;
    QSlider* seekSlider_;
    VolumeDial* volumeDial_;
    QLabel* timeLabel_;
    bool muted_ = false;
    int volumeBeforeMute_ = 100;
};
