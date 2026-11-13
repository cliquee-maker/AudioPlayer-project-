#pragma once

#include <QElapsedTimer>
#include <QPixmap>
#include <QWidget>

class QPainter;
class QPaintEvent;
class QRectF;
class QTimer;

// Обложка текущего трека.
// Обычный режим — статичная квадратная обложка.
// Режим винила — неподвижная пластинка, внутри которой вращается обложка
// (как в Gran Turismo 7). Вращение идёт только во время воспроизведения.
class AlbumCoverWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlbumCoverWidget(QWidget* parent = nullptr);

    void setCover(const QPixmap& cover);
    void clearCover();

    void setVinylMode(bool enabled);
    bool vinylMode() const;

    void setSpinning(bool spinning);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void advanceRotation();
    void paintStaticCover(QPainter& painter, const QRectF& target);
    void paintVinyl(QPainter& painter, const QRectF& target);

    QPixmap cover_;
    QPixmap placeholder_;
    QPixmap vinyl_;
    QElapsedTimer clock_;
    QTimer* spinTimer_ = nullptr;
    bool vinylMode_ = false;
    bool spinning_ = false;
    qreal angle_ = 0.0;
};
