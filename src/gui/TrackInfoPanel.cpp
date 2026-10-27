#include "gui/TrackInfoPanel.hpp"

#include "library/Playlist.hpp"
#include "library/Track.hpp"

#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

namespace {

QPixmap placeholderCover(int size) {
    QPixmap pm(size, size);
    pm.fill(QColor(70, 70, 74));
    QPainter painter(&pm);
    painter.setPen(QColor(160, 160, 166));
    QFont font = painter.font();
    font.setPixelSize(size / 2);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("♪"));
    return pm;
}

constexpr int kCoverSize = 240;

}  // namespace 

TrackInfoPanel::TrackInfoPanel(Playlist& playlist, QWidget* parent)
    : QWidget(parent), playlist_(playlist) {
    coverLabel_ = new QLabel(this);
    coverLabel_->setFixedSize(kCoverSize, kCoverSize);
    coverLabel_->setAlignment(Qt::AlignCenter);
    coverLabel_->setScaledContents(true);
    coverLabel_->setPixmap(placeholderCover(kCoverSize));

    titleLabel_ = new QLabel(tr("—"), this);
    titleLabel_->setWordWrap(true);
    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);

    artistLabel_ = new QLabel(this);
    artistLabel_->setWordWrap(true);
    albumLabel_ = new QLabel(this);
    albumLabel_->setWordWrap(true);
    albumLabel_->setStyleSheet(QStringLiteral("color: gray;"));

    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(titleLabel_);
    text->addWidget(artistLabel_);
    text->addWidget(albumLabel_);
    text->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(coverLabel_, 0, Qt::AlignLeft);
    layout->addLayout(text, 1);

    connect(&playlist_, &Playlist::currentChanged, this, &TrackInfoPanel::refresh);
    connect(&playlist_, &Playlist::changed, this, &TrackInfoPanel::refresh);
    refresh();
}

void TrackInfoPanel::refresh() {
    const Track* track = playlist_.current();
    if (track == nullptr) {
        coverLabel_->setPixmap(placeholderCover(kCoverSize));
        titleLabel_->setText(tr("—"));
        artistLabel_->clear();
        albumLabel_->clear();
        return;
    }

    QPixmap cover;
    if (!track->coverData.isEmpty() && cover.loadFromData(track->coverData)) {
        coverLabel_->setPixmap(cover);
    } else {
        coverLabel_->setPixmap(placeholderCover(kCoverSize));
    }

    QString title = track->title;
    if (title.isEmpty()) title = QString::fromStdString(track->path.filename().string());
    titleLabel_->setText(title);
    artistLabel_->setText(track->artist);
    albumLabel_->setText(track->album);
}
