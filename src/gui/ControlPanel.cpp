#include "gui/ControlPanel.hpp"

#include "audio/AudioEngine.hpp"
#include "gui/VolumeDial.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
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

ControlPanel::ControlPanel(AudioEngine& engine, Playlist& playlist, QWidget* parent,
                           QWidget* visualizer)
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

    repeatBtn_ = new QToolButton(this);
    repeatBtn_->setText(QStringLiteral("↻"));
    repeatBtn_->setCheckable(true);
    repeatBtn_->setToolTip(tr("Повтор: выкл"));

    effectBtn_ = new QToolButton(this);
    effectBtn_->setText(QStringLiteral("✦"));
    effectBtn_->setToolTip(tr("Эффект: Полосы"));

    shuffleBtn_ = new QToolButton(this);
    shuffleBtn_->setText(QStringLiteral("🔀"));
    shuffleBtn_->setCheckable(true);
    shuffleBtn_->setToolTip(tr("Перемешать"));

    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 0);

    timeLabel_ = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    timeLabel_->setMinimumWidth(110);

    volumeDial_ = new VolumeDial(this);
    volumeDial_->setRange(0, 100);
    volumeDial_->setValue(static_cast<int>(engine_.volume()));
    volumeDial_->setFixedSize(volumeDial_->sizeHint());

    auto* playbackButtons = new QGridLayout;
    playbackButtons->setSpacing(6);
    playbackButtons->addWidget(prevBtn_, 0, 0);
    playbackButtons->addWidget(playBtn_, 0, 1);
    playbackButtons->addWidget(stopBtn_, 0, 2);
    playbackButtons->addWidget(nextBtn_, 0, 3);
    playbackButtons->addWidget(repeatBtn_, 1, 0);
    playbackButtons->addWidget(effectBtn_, 1, 1);
    playbackButtons->addWidget(shuffleBtn_, 1, 2);

    auto* seekRow = new QHBoxLayout;
    seekRow->setSpacing(6);
    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeLabel_);

    auto* center = new QVBoxLayout;
    center->setSpacing(4);
    center->addLayout(seekRow);
    if (visualizer != nullptr) {
        center->addWidget(visualizer, 1);
    } else {
        center->addStretch(1);
    }

    auto* volumeLabel = new QLabel(QStringLiteral("Громкость"), this);
    volumeLabel->setAlignment(Qt::AlignCenter);
    auto* volumeBox = new QVBoxLayout;
    volumeBox->setSpacing(2);
    volumeBox->addWidget(volumeLabel);
    volumeBox->addWidget(volumeDial_, 0, Qt::AlignHCenter);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);
    layout->addLayout(playbackButtons, 0);
    layout->addLayout(center, 1);
    layout->addLayout(volumeBox, 0);

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
    connect(volumeDial_, &VolumeDial::valueChanged, this, [this](int value) {
        engine_.setVolume(static_cast<float>(value));
        volumeDial_->setToolTip(tr("Громкость: %1%").arg(value));
    });
    volumeDial_->setToolTip(tr("Громкость: %1%").arg(volumeDial_->value()));

    connect(repeatBtn_, &QToolButton::clicked, this, &ControlPanel::onRepeatClicked);
    connect(effectBtn_, &QToolButton::clicked, this,
            &ControlPanel::nextEffectRequested);
    connect(shuffleBtn_, &QToolButton::toggled, this,
            [this](bool on) { playlist_.setShuffle(on); });

    connect(&engine_, &AudioEngine::positionChanged, this, &ControlPanel::onPositionChanged);
    connect(&engine_, &AudioEngine::durationChanged, this, &ControlPanel::onDurationChanged);
    connect(&engine_, &AudioEngine::stateChanged, this, &ControlPanel::onStateChanged);
    connect(&engine_, &AudioEngine::mediaFinished, this, &ControlPanel::handleMediaFinished);

    refreshRepeatButton();
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

void ControlPanel::setEffectName(const QString& name) {
    effectBtn_->setToolTip(tr("Эффект: %1").arg(name));
}

void ControlPanel::refreshRepeatButton() {
    switch (playlist_.repeat()) {
    case Playlist::Repeat::Off:
        repeatBtn_->setChecked(false);
        repeatBtn_->setToolTip(tr("Повтор: выкл"));
        break;
    case Playlist::Repeat::Track:
        repeatBtn_->setChecked(true);
        repeatBtn_->setToolTip(tr("Повтор: трек"));
        break;
    case Playlist::Repeat::Playlist:
        repeatBtn_->setChecked(true);
        repeatBtn_->setToolTip(tr("Повтор: список"));
        break;
    }
}

void ControlPanel::refreshTimeLabel(qint64 shownMs) {
    timeLabel_->setText(formatMs(shownMs) + QStringLiteral(" / ") +
                        formatMs(engine_.durationMs()));
}
