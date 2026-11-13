#include "gui/TrackInfoPanel.hpp"

#include "gui/AlbumCoverWidget.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"

#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

namespace {

constexpr int kCoverSize = 240;

}  // namespace

TrackInfoPanel::TrackInfoPanel(Playlist& playlist, QWidget* parent)
    : QWidget(parent), playlist_(playlist) {
    cover_ = new AlbumCoverWidget(this);
    cover_->setFixedSize(kCoverSize, kCoverSize);

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
    layout->addWidget(cover_, 0, Qt::AlignLeft);
    layout->addLayout(text, 1);

    connect(&playlist_, &Playlist::currentChanged, this, &TrackInfoPanel::refresh);
    connect(&playlist_, &Playlist::changed, this, &TrackInfoPanel::refresh);
    refresh();
}

void TrackInfoPanel::setVinylCover(bool enabled) {
    cover_->setVinylMode(enabled);
}

void TrackInfoPanel::setSpinning(bool spinning) {
    cover_->setSpinning(spinning);
}

void TrackInfoPanel::refresh() {
    const Track* track = playlist_.current();
    if (track == nullptr) {
        cover_->clearCover();
        titleLabel_->setText(tr("—"));
        artistLabel_->clear();
        albumLabel_->clear();
        return;
    }

    QPixmap cover;
    if (!track->coverData.isEmpty() && cover.loadFromData(track->coverData)) {
        cover_->setCover(cover);
    } else {
        cover_->clearCover();
    }

    QString title = track->title;
    if (title.isEmpty()) title = QString::fromStdString(track->path.filename().string());
    titleLabel_->setText(title);
    artistLabel_->setText(track->artist);
    albumLabel_->setText(track->album);
}
