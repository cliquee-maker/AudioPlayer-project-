#pragma once

#include <QWidget>

class AlbumCoverWidget;
class QLabel;
class Playlist;

// Правая верхняя область: обложка и информация о текущем треке.
class TrackInfoPanel : public QWidget {
    Q_OBJECT
public:
    explicit TrackInfoPanel(Playlist& playlist, QWidget* parent = nullptr);

    // Обложка в виде пластинки с вращающейся внутри картинкой.
    void setVinylCover(bool enabled);
    // Вращение обложки (только когда трек играет).
    void setSpinning(bool spinning);

private slots:
    void refresh();

private:
    Playlist& playlist_;
    AlbumCoverWidget* cover_;
    QLabel* titleLabel_;
    QLabel* artistLabel_;
    QLabel* albumLabel_;
};
