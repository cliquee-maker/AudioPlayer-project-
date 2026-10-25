#pragma once

#include <QWidget>

class AudioEngine;
class Playlist;
class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
class QToolButton;

class ControlPanel : public QWidget {
    Q_OBJECT
public:
    ControlPanel(AudioEngine& engine, Playlist& playlist, QWidget* parent = nullptr);

public slots:
    void next();
    void prev();
    void handleMediaFinished();
    void setEffectName(const QString& name);

signals:
    void nextEffectRequested();

private slots:
    void onStateChanged(bool playing);
    void onPositionChanged(qint64 ms);
    void onDurationChanged(qint64 ms);
    void onCurrentChanged(int index);
    void onSeekMoved(int ms);
    void onSeekReleased();
    void onRepeatClicked();

private:
    void refreshTitle();
    void refreshRepeatButton();
    void refreshTimeLabel(qint64 shownMs);

    AudioEngine& engine_;
    Playlist& playlist_;

    QToolButton* prevBtn_;
    QToolButton* playBtn_;
    QToolButton* stopBtn_;
    QToolButton* nextBtn_;
    QPushButton* repeatBtn_;
    QPushButton* effectBtn_;
    QCheckBox* shuffleBox_;
    QSlider* seekSlider_;
    QSlider* volumeSlider_;
    QLabel* timeLabel_;
    QLabel* titleLabel_;
};
