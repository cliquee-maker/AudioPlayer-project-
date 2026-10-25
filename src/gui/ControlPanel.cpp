#include "gui/ControlPanel.hpp"

#include "audio/AudioEngine.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString formatMs(qint64 ms) {
    const qint64 total = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

ControlPanel::ControlPanel(AudioEngine& engine, Playlist& playlist, QWidget* parent)
    : QWidget(parent), engine_(engine), playlist_(playlist) {
    prevBtn_ = new QToolButton(this);
    prevBtn_->setText(QStringLiteral("⏮"));
    playBtn_ = new QToolButton(this);
    playBtn_->setText(QStringLiteral("▶"));
    playBtn_->setObjectName(QStringLiteral("playButton"));
    stopBtn_ = new QToolButton(this);
    stopBtn_->setText(QStringLiteral("⏹"));
    nextBtn_ = new QToolButton(this);
    nextBtn_->setText(QStringLiteral("⏭"));

    repeatBtn_ = new QPushButton(this);
    shuffleBox_ = new QCheckBox(QStringLiteral("Перемешать"), this);

    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 0);

    timeLabel_ = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    timeLabel_->setMinimumWidth(110);

    titleLabel_ = new QLabel(QStringLiteral("—"), this);

    volumeSlider_ = new QSlider(Qt::Horizontal, this);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(static_cast<int>(engine_.volume()));
    volumeSlider_->setFixedWidth(140);

    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    top->addWidget(prevBtn_);
    top->addWidget(playBtn_);
    top->addWidget(stopBtn_);
    top->addWidget(nextBtn_);
    top->addSpacing(10);
    top->addWidget(repeatBtn_);
    top->addWidget(shuffleBox_);
    top->addSpacing(10);
    top->addWidget(seekSlider_, 1);
    top->addWidget(timeLabel_);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(titleLabel_, 1);
    bottom->addWidget(new QLabel(QStringLiteral("Громкость"), this));
    bottom->addWidget(volumeSlider_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(top);
    layout->addLayout(bottom);

    connect(prevBtn_, &QToolButton::clicked, this, &ControlPanel::prev);
    connect(nextBtn_, &QToolButton::clicked, this, &ControlPanel::next);
    connect(playBtn_, &QToolButton::clicked, this, [this] {
        if (engine_.isPlaying()) {
            engine_.pause();
        } else {
            engine_.play();
        }
    });
    connect(stopBtn_, &QToolButton::clicked, &engine_, &AudioEngine::stop);

    connect(seekSlider_, &QSlider::sliderMoved, this, &ControlPanel::onSeekMoved);
    connect(seekSlider_, &QSlider::sliderReleased, this, &ControlPanel::onSeekReleased);
    connect(volumeSlider_, &QSlider::valueChanged, this,
            [this](int value) { engine_.setVolume(static_cast<float>(value)); });

    connect(repeatBtn_, &QPushButton::clicked, this, &ControlPanel::onRepeatClicked);
    connect(shuffleBox_, &QCheckBox::toggled, this,
            [this](bool on) { playlist_.setShuffle(on); });

    connect(&engine_, &AudioEngine::positionChanged, this, &ControlPanel::onPositionChanged);
    connect(&engine_, &AudioEngine::durationChanged, this, &ControlPanel::onDurationChanged);
    connect(&engine_, &AudioEngine::stateChanged, this, &ControlPanel::onStateChanged);
    connect(&engine_, &AudioEngine::mediaFinished, this, &ControlPanel::handleMediaFinished);

    connect(&playlist_, &Playlist::currentChanged, this, &ControlPanel::onCurrentChanged);
    connect(&playlist_, &Playlist::changed, this, [this] { refreshTitle(); });

    refreshRepeatButton();
    refreshTitle();
    refreshTimeLabel(0);
}

void ControlPanel::next() {
    const int size = playlist_.size();
    if (size == 0) return;
    int index = playlist_.currentIndex();
    if (playlist_.shuffle() && size > 1) {
        do {
            index = QRandomGenerator::global()->bounded(size);
        } while (index == playlist_.currentIndex());
    } else {
        index = (index + 1) % size;
    }
    playlist_.setCurrentIndex(index);
}

void ControlPanel::prev() {
    const int size = playlist_.size();
    if (size == 0) return;
    int index = playlist_.currentIndex();
    if (index < 0) index = size - 1;
    playlist_.setCurrentIndex((index - 1 + size) % size);
}

void ControlPanel::handleMediaFinished() {
    if (playlist_.size() == 0) return;
    if (playlist_.repeat() == Playlist::Repeat::Track) {
        engine_.seek(0);
        engine_.play();
        return;
    }
    const int before = playlist_.currentIndex();
    next();
    if (playlist_.currentIndex() == before) engine_.seek(0);
    engine_.play();
}

void ControlPanel::onStateChanged(bool playing) {
    playBtn_->setText(playing ? QStringLiteral("⏸") : QStringLiteral("▶"));
}

void ControlPanel::onPositionChanged(qint64 ms) {
    if (!seekSlider_->isSliderDown()) seekSlider_->setValue(static_cast<int>(ms));
    refreshTimeLabel(seekSlider_->isSliderDown() ? seekSlider_->value() : ms);
}

void ControlPanel::onDurationChanged(qint64 ms) {
    seekSlider_->setRange(0, static_cast<int>(ms));
    refreshTimeLabel(seekSlider_->value());
}

void ControlPanel::onCurrentChanged(int) {
    refreshTitle();
}

void ControlPanel::onSeekMoved(int ms) {
    refreshTimeLabel(ms);
}

void ControlPanel::onSeekReleased() {
    engine_.seek(seekSlider_->value());
}

void ControlPanel::onRepeatClicked() {
    switch (playlist_.repeat()) {
    case Playlist::Repeat::Off:
        playlist_.setRepeat(Playlist::Repeat::Track);
        break;
    case Playlist::Repeat::Track:
        playlist_.setRepeat(Playlist::Repeat::Playlist);
        break;
    case Playlist::Repeat::Playlist:
        playlist_.setRepeat(Playlist::Repeat::Off);
        break;
    }
    refreshRepeatButton();
}

void ControlPanel::refreshTitle() {
    const Track* track = playlist_.current();
    if (track == nullptr) {
        titleLabel_->setText(QStringLiteral("—"));
        return;
    }
    titleLabel_->setText(track->title + QStringLiteral(" — ") + track->artist);
}

void ControlPanel::refreshRepeatButton() {
    switch (playlist_.repeat()) {
    case Playlist::Repeat::Off:
        repeatBtn_->setText(QStringLiteral("Повтор: выкл"));
        break;
    case Playlist::Repeat::Track:
        repeatBtn_->setText(QStringLiteral("Повтор: трек"));
        break;
    case Playlist::Repeat::Playlist:
        repeatBtn_->setText(QStringLiteral("Повтор: список"));
        break;
    }
}

void ControlPanel::refreshTimeLabel(qint64 shownMs) {
    timeLabel_->setText(formatMs(shownMs) + QStringLiteral(" / ") +
                        formatMs(engine_.durationMs()));
}
