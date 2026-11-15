#include "gui/AlbumCoverWidget.hpp"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

// Вращение намеренно медленнее реальных 33⅓ об/мин: так спокойнее
// и не отвлекает от интерфейса (полный оборот ~4 секунды).
constexpr qreal kDegreesPerSecond = 90.0;
// Радиус диска относительно стороны квадрата виджета
// (у исходной картинки пластинки есть прозрачные поля).
constexpr qreal kDiscRadiusRatio = 0.468;
// Радиус обложки относительно радиуса пластинки. Заметно больше
// белой этикетки — обложка занимает половину диаметра винила.
constexpr qreal kCoverRadiusRatio = 0.50;

QPixmap makePlaceholder() {
    constexpr int kSize = 512;
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRadialGradient gradient(kSize / 2.0, kSize / 2.0, kSize / 2.0);
    gradient.setColorAt(0.0, QColor(96, 120, 190));
    gradient.setColorAt(1.0, QColor(42, 50, 76));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(QRectF(0, 0, kSize, kSize));

    // Пара смещённых дуг: без них вращение однотонной заглушки незаметно.
    painter.setBrush(Qt::NoBrush);
    QPen ring(QColor(255, 255, 255, 45), kSize * 0.02);
    painter.setPen(ring);
    for (int i = 0; i < 4; ++i) {
        painter.drawArc(QRectF(kSize * 0.10, kSize * 0.10, kSize * 0.80, kSize * 0.80),
                        i * 90 * 16 + 15 * 16, 55 * 16);
    }

    QFont font = painter.font();
    font.setPixelSize(kSize / 3);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(240, 240, 245));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("♪"));

    return pixmap;
}

QRectF centerCropSource(const QPixmap& pixmap) {
    const qreal side = std::min(pixmap.width(), pixmap.height());
    return QRectF((pixmap.width() - side) / 2.0, (pixmap.height() - side) / 2.0, side, side);
}

}  // namespace

AlbumCoverWidget::AlbumCoverWidget(QWidget* parent)
    : QWidget(parent),
      placeholder_(makePlaceholder()),
      vinyl_(QStringLiteral(":/assets/vinyl-record.png")) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    spinTimer_ = new QTimer(this);
    spinTimer_->setInterval(16);
    connect(spinTimer_, &QTimer::timeout, this, &AlbumCoverWidget::advanceRotation);
}

void AlbumCoverWidget::setCover(const QPixmap& cover) {
    cover_ = cover;
    update();
}

void AlbumCoverWidget::clearCover() {
    cover_ = QPixmap();
    update();
}

void AlbumCoverWidget::setVinylMode(bool enabled) {
    if (vinylMode_ == enabled) return;
    vinylMode_ = enabled;

    if (vinylMode_ && spinning_ && !vinyl_.isNull()) {
        clock_.restart();
        spinTimer_->start();
    } else {
        spinTimer_->stop();
    }
    update();
}

bool AlbumCoverWidget::vinylMode() const {
    return vinylMode_;
}

void AlbumCoverWidget::setSpinning(bool spinning) {
    if (spinning_ == spinning) return;
    spinning_ = spinning;

    if (spinning_ && vinylMode_ && !vinyl_.isNull()) {
        clock_.restart();
        spinTimer_->start();
    } else {
        spinTimer_->stop();
    }
}

void AlbumCoverWidget::advanceRotation() {
    if (!clock_.isValid()) {
        clock_.start();
        return;
    }
    const qint64 elapsedMs = clock_.restart();
    angle_ = std::fmod(angle_ + static_cast<qreal>(elapsedMs) * kDegreesPerSecond / 1000.0,
                       360.0);
    update();
}

void AlbumCoverWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal side = std::min(width(), height());
    if (side <= 0.0) return;
    const QRectF target((width() - side) / 2.0, (height() - side) / 2.0, side, side);

    if (vinylMode_ && !vinyl_.isNull()) {
        paintVinyl(painter, target);
    } else {
        paintStaticCover(painter, target);
    }
}

void AlbumCoverWidget::paintStaticCover(QPainter& painter, const QRectF& target) {
    const QPixmap& pixmap = cover_.isNull() ? placeholder_ : cover_;
    if (pixmap.isNull()) return;

    painter.save();
    painter.setClipRect(target);
    painter.drawPixmap(target, pixmap, centerCropSource(pixmap));
    painter.restore();
}

void AlbumCoverWidget::paintVinyl(QPainter& painter, const QRectF& target) {
    painter.drawPixmap(target, vinyl_, QRectF(vinyl_.rect()));

    const QPointF center = target.center();
    const qreal discRadius = target.width() * kDiscRadiusRatio;
    const qreal coverRadius = discRadius * kCoverRadiusRatio;

    const QPixmap& pixmap = cover_.isNull() ? placeholder_ : cover_;
    if (!pixmap.isNull()) {
        painter.save();
        QPainterPath clip;
        clip.addEllipse(center, coverRadius, coverRadius);
        painter.setClipPath(clip);

        // Крутится только картинка; сама пластинка остаётся неподвижной.
        painter.translate(center);
        painter.rotate(angle_);
        const QRectF coverRect(-coverRadius, -coverRadius, coverRadius * 2.0,
                               coverRadius * 2.0);
        painter.drawPixmap(coverRect, pixmap, centerCropSource(pixmap));
        painter.restore();
    }

    // Отверстие шпинделя поверх обложки.
    const qreal holeRadius = std::max(2.0, discRadius * 0.018);
    painter.setPen(QPen(QColor(205, 205, 212), std::max(1.0, holeRadius * 0.45)));
    painter.setBrush(QColor(22, 22, 24));
    painter.drawEllipse(center, holeRadius, holeRadius);
}
