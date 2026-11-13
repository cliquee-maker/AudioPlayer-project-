#include "gui/MainWindow.hpp"

#include "audio/AudioEngine.hpp"
#include "gui/ControlPanel.hpp"
#include "gui/LibraryPanel.hpp"
#include "gui/LyricsPanel.hpp"
#include "gui/QueuePanel.hpp"
#include "gui/RecommendationsPanel.hpp"
#include "gui/TrackInfoPanel.hpp"
#include "gui/Visualizer.hpp"
#include "library/MusicLibrary.hpp"
#include "library/Playlist.hpp"
#include "library/Track.hpp"
#include "stats/ListeningStats.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(AudioEngine& engine, MusicLibrary& library, Playlist& playlist,
                       ListeningStats& stats, QWidget* parent)
    : QMainWindow(parent),
      engine_(engine),
      library_(library),
      playlist_(playlist),
      stats_(stats),
      visualSource_(engine) {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    visualizer_ = new Visualizer(&visualSource_, central);
    control_ = new ControlPanel(engine_, playlist_, central, visualizer_);

    // Плейлист подхватывает содержимое библиотеки, пока пуст.
    // Connect должен быть ДО создания LibraryPanel: его конструктор
    // сканирует последнюю папку и сразу эмитит libraryChanged.
    connect(&library_, &MusicLibrary::libraryChanged, this, [this] {
        if (playlist_.size() > 0) return;
        for (const Track& track : library_.tracks()) playlist_.add(track);
        if (playlist_.size() > 0) playlist_.setCurrentIndex(0);
    });

    // Библиотека занимает пустой участок верхнего ряда (слева),
    // справа — колонка «обложка + инфо» и под ней текст песни.
    // «Рекомендации» — вторая страница того же участка: каталог песен
    // скрывается, вместо него показываются папки и текст из API.
    auto* libraryPanel = new LibraryPanel(library_, central);
    auto* recommendationsPanel = new RecommendationsPanel(central);
    auto* contentStack = new QStackedWidget(central);
    contentStack->addWidget(libraryPanel);
    contentStack->addWidget(recommendationsPanel);

    // --- Меню в левом верхнем углу окна ---
    auto* musicMenu = menuBar()->addMenu(tr("Музыка"));

    auto* openFileAction = musicMenu->addAction(tr("Открыть файл…"));
    openFileAction->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    openFileAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    connect(openFileAction, &QAction::triggered, this, [this] {
        QSettings settings;
        const QString start =
            settings.value(QStringLiteral("library/folder"), QDir::homePath()).toString();
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Открыть аудиофайл"), start,
            tr("Аудиофайлы (*.mp3 *.wav *.ogg *.flac);;Все файлы (*)"));
        if (file.isEmpty()) return;

        const auto path = std::filesystem::u8path(file.toStdString());

        int index = -1;
        for (int i = 0; i < playlist_.size(); ++i) {
            if (playlist_.at(i).path == path) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            Track track;
            bool found = false;
            for (const Track& known : library_.tracks()) {
                if (known.path == path) {
                    track = known;
                    found = true;
                    break;
                }
            }
            if (!found) {
                track.path = path;
                track.title = QFileInfo(file).completeBaseName();
            }
            playlist_.add(track);
            index = playlist_.size() - 1;
        }
        playlist_.setCurrentIndex(index);
        engine_.play();
    });

    musicMenu->addSeparator();

    auto* prevAction = musicMenu->addAction(tr("Предыдущий трек"));
    prevAction->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    prevAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(prevAction, &QAction::triggered, control_, &ControlPanel::prev);

    auto* playAction = musicMenu->addAction(tr("Играть"));
    playAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    playAction->setShortcut(QKeySequence(QStringLiteral("F6")));
    connect(playAction, &QAction::triggered, this, [this] {
        if (engine_.isPlaying()) {
            engine_.pause();
        } else {
            engine_.play();
        }
    });
    connect(&engine_, &AudioEngine::stateChanged, playAction, [this, playAction](bool playing) {
        playAction->setText(playing ? tr("Пауза") : tr("Играть"));
        playAction->setIcon(style()->standardIcon(
            playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    });

    auto* stopAction = musicMenu->addAction(tr("Стоп"));
    stopAction->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stopAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    connect(stopAction, &QAction::triggered, &engine_, &AudioEngine::stop);

    auto* nextAction = musicMenu->addAction(tr("Следующий трек"));
    nextAction->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    nextAction->setShortcut(QKeySequence(QStringLiteral("F8")));
    connect(nextAction, &QAction::triggered, control_, &ControlPanel::next);

    musicMenu->addSeparator();

    auto* muteAction = musicMenu->addAction(tr("Приглушить звук"));
    muteAction->setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
    muteAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    muteAction->setCheckable(true);
    connect(muteAction, &QAction::toggled, control_, &ControlPanel::setMuted);

    auto* favoriteAction = musicMenu->addAction(tr("В любимые"));
    favoriteAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    favoriteAction->setEnabled(false);

    musicMenu->addSeparator();

    auto* quitAction = musicMenu->addAction(tr("Выход"));
    quitAction->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto* playlistMenu = menuBar()->addMenu(tr("Плейлист"));
    auto* openFolderAction = playlistMenu->addAction(tr("Открыть папку…"));
    openFolderAction->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(openFolderAction, &QAction::triggered, libraryPanel, &LibraryPanel::chooseFolder);
    playlistMenu->addSeparator();
    auto* clearPlaylistAction = playlistMenu->addAction(tr("Очистить плейлист"));
    connect(clearPlaylistAction, &QAction::triggered, &playlist_, &Playlist::clear);

    auto* serviceMenu = menuBar()->addMenu(tr("Сервис"));
    auto* recommendationsAction = serviceMenu->addAction(tr("Рекомендации"));
    recommendationsAction->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    recommendationsAction->setCheckable(true);
    recommendationsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(recommendationsAction, &QAction::toggled, this, [contentStack](bool show) {
        contentStack->setCurrentIndex(show ? 1 : 0);
    });
    connect(recommendationsPanel, &RecommendationsPanel::backRequested, recommendationsAction,
            [recommendationsAction] { recommendationsAction->setChecked(false); });

    serviceMenu->addSeparator();
    auto* nextEffectAction = serviceMenu->addAction(tr("Следующий эффект визуализации"));
    connect(nextEffectAction, &QAction::triggered, visualizer_, &Visualizer::cycleMode);

    auto* equalizerAction = serviceMenu->addAction(tr("Эквалайзер…"));
    equalizerAction->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
    connect(equalizerAction, &QAction::triggered, this, [this] {
        QMessageBox::information(this, tr("Эквалайзер"),
                                 tr("Эквалайзер пока не реализован."));
    });

    auto* personalizationMenu = menuBar()->addMenu(tr("Персонализация"));
    auto* volumeStyleGroup = new QActionGroup(this);
    volumeStyleGroup->setExclusive(true);

    auto* dialAction = personalizationMenu->addAction(tr("Круглая ручка громкости"));
    dialAction->setCheckable(true);
    auto* sliderAction = personalizationMenu->addAction(tr("Обычный ползунок громкости"));
    sliderAction->setCheckable(true);
    volumeStyleGroup->addAction(dialAction);
    volumeStyleGroup->addAction(sliderAction);

    QSettings settings;
    const bool useDial =
        settings.value(QStringLiteral("personalization/volumeDial"), false).toBool();
    (useDial ? dialAction : sliderAction)->setChecked(true);
    control_->setVolumeControlStyle(useDial);

    connect(volumeStyleGroup, &QActionGroup::triggered, this,
            [this, dialAction](QAction* action) {
                const bool dial = (action == dialAction);
                control_->setVolumeControlStyle(dial);
                QSettings().setValue(QStringLiteral("personalization/volumeDial"), dial);
            });

    personalizationMenu->addSeparator();
    auto* coverStyleGroup = new QActionGroup(this);
    coverStyleGroup->setExclusive(true);

    auto* staticCoverAction = personalizationMenu->addAction(tr("Обычная обложка"));
    staticCoverAction->setCheckable(true);
    auto* vinylCoverAction = personalizationMenu->addAction(tr("Крутящийся винил (обложка)"));
    vinylCoverAction->setCheckable(true);
    coverStyleGroup->addAction(staticCoverAction);
    coverStyleGroup->addAction(vinylCoverAction);

    const bool useVinylCover =
        settings.value(QStringLiteral("personalization/vinylCover"), false).toBool();
    (useVinylCover ? vinylCoverAction : staticCoverAction)->setChecked(true);
    connect(coverStyleGroup, &QActionGroup::triggered, this,
            [this, vinylCoverAction](QAction* action) {
                const bool vinyl = (action == vinylCoverAction);
                if (trackInfo_ != nullptr) trackInfo_->setVinylCover(vinyl);
                QSettings().setValue(QStringLiteral("personalization/vinylCover"), vinyl);
            });

    auto* helpMenu = menuBar()->addMenu(tr("Справка"));
    auto* aboutAction = helpMenu->addAction(tr("О программе"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("О программе"),
                           tr("<b>LOCAL AUDIO PLAYER</b><br>"
                              "Локальный аудиоплеер с библиотекой, плейлистами "
                              "и визуализацией."));
    });

    // Правая колонка фиксированной ширины: текст переносится внутри поля,
    // а не расширяет его.
    auto* infoBox = new QWidget(central);
    infoBox->setFixedWidth(260);
    auto* infoColumn = new QVBoxLayout(infoBox);
    infoColumn->setContentsMargins(0, 0, 0, 0);
    infoColumn->setSpacing(8);
    trackInfo_ = new TrackInfoPanel(playlist_, infoBox);
    infoColumn->addWidget(trackInfo_, 0);

    // Под треком — вкладки «Текст песни» / «Очередь». Свернуть/развернуть
    // блок можно через контекстное меню по правой кнопке.
    auto* detailsPlaceholder =
        new QLabel(QStringLiteral("▸  ") + tr("Текст песни / Очередь"), infoBox);
    detailsPlaceholder->setStyleSheet(QStringLiteral("color: gray;"));
    detailsPlaceholder->setToolTip(tr("ПКМ — развернуть"));
    detailsPlaceholder->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* detailTabs = new QTabWidget(infoBox);
    detailTabs->setDocumentMode(true);
    auto* lyricsPanel = new LyricsPanel(detailTabs);
    auto* queuePanel = new QueuePanel(detailTabs);
    detailTabs->addTab(lyricsPanel, tr("Текст песни"));
    detailTabs->addTab(queuePanel, tr("Очередь"));

    auto* detailTabBar = detailTabs->tabBar();
    detailTabBar->setToolTip(tr("ПКМ — свернуть"));
    detailTabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    detailTabs->setContextMenuPolicy(Qt::CustomContextMenu);

    infoColumn->addWidget(detailsPlaceholder, 0);
    infoColumn->addWidget(detailTabs, 1);
    // Когда вкладки свёрнуты, свободное место забирает этот распорка,
    // а заголовок остаётся сразу под информацией о треке.
    infoColumn->addSpacerItem(
        new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // По умолчанию блок всегда развёрнут при запуске.
    detailsPlaceholder->setVisible(false);
    detailTabs->setVisible(true);

    const auto applyDetailsExpanded = [detailsPlaceholder, detailTabs](bool expanded) {
        detailTabs->setVisible(expanded);
        detailsPlaceholder->setVisible(!expanded);
    };

    auto* detailsMenu = new QMenu(this);
    QAction* detailsToggleAction = detailsMenu->addAction(tr("Свернуть"));

    const auto showDetailsMenu = [detailsMenu, detailsToggleAction, detailTabs,
                                  applyDetailsExpanded](const QPoint& globalPos) {
        const bool expanded = detailTabs->isVisible();
        detailsToggleAction->setText(expanded ? tr("Свернуть") : tr("Развернуть"));
        if (detailsMenu->exec(globalPos) != detailsToggleAction) return;
        applyDetailsExpanded(!expanded);
    };

    connect(detailsPlaceholder, &QWidget::customContextMenuRequested, this,
            [showDetailsMenu, detailsPlaceholder](const QPoint& pos) {
                showDetailsMenu(detailsPlaceholder->mapToGlobal(pos));
            });
    connect(detailTabBar, &QWidget::customContextMenuRequested, this,
            [showDetailsMenu, detailTabBar](const QPoint& pos) {
                showDetailsMenu(detailTabBar->mapToGlobal(pos));
            });
    connect(detailTabs, &QWidget::customContextMenuRequested, this,
            [showDetailsMenu, detailTabs](const QPoint& pos) {
                showDetailsMenu(detailTabs->mapToGlobal(pos));
            });
    connect(lyricsPanel, &LyricsPanel::collapseRequested, this,
            [applyDetailsExpanded] { applyDetailsExpanded(false); });
    connect(queuePanel, &QueuePanel::collapseRequested, this,
            [applyDetailsExpanded] { applyDetailsExpanded(false); });

    // Обложка-винил крутится только во время воспроизведения.
    trackInfo_->setVinylCover(
        settings.value(QStringLiteral("personalization/vinylCover"), false).toBool());
    trackInfo_->setSpinning(engine_.isPlaying());
    connect(&engine_, &AudioEngine::stateChanged, trackInfo_, [this](bool playing) {
        trackInfo_->setSpinning(playing);
    });

    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(10);
    topRow->addWidget(contentStack, 1);
    topRow->addWidget(infoBox, 0);

    layout->addLayout(topRow, 1);
    layout->addWidget(control_);
    setCentralWidget(central);

    connect(&playlist_, &Playlist::currentChanged, this, [this](int index) {
        if (index < 0 || index >= playlist_.size()) return;
        visualSource_.setTrack(playlist_.at(index).path);
        engine_.open(playlist_.at(index).path);
    });

    connect(control_, &ControlPanel::nextEffectRequested, visualizer_,
            &Visualizer::cycleMode);
    connect(visualizer_, &Visualizer::modeChanged, control_, [this] {
        control_->setEffectName(visualizer_->modeName());
    });
    control_->setEffectName(visualizer_->modeName());

    connect(libraryPanel, &LibraryPanel::trackActivated, this, [this](int index) {
        const auto& tracks = library_.tracks();
        if (index < 0 || index >= static_cast<int>(tracks.size())) return;
        const Track& track = tracks[static_cast<std::size_t>(index)];

        int playlistIndex = -1;
        for (int i = 0; i < playlist_.size(); ++i) {
            if (playlist_.at(i).path == track.path) {
                playlistIndex = i;
                break;
            }
        }
        if (playlistIndex == -1) {
            playlist_.add(track);
            playlistIndex = playlist_.size() - 1;
        }
        playlist_.setCurrentIndex(playlistIndex);
        engine_.play();
    });

    if (const Track* track = playlist_.current()) {
        visualSource_.setTrack(track->path);
        engine_.open(track->path);
    }

    setWindowTitle(QStringLiteral("LOCAL AUDIO PLAYER"));
    resize(760, 560);
}
