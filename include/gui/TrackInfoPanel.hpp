#pragma once

#include <QWidget>

class QLabel;
class Playlist;

// Правая верхняя область: обложка и информация о текущем треке.
class TrackInfoPanel : public QWidget {
    Q_OBJECT
public:
    explicit TrackInfoPanel(Playlist& playlist, QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    Playlist& playlist_;
    QLabel* coverLabel_;
    QLabel* titleLabel_;
    QLabel* artistLabel_;
    QLabel* albumLabel_;
};
