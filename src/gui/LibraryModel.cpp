#include "gui/LibraryModel.hpp"

#include "library/MusicLibrary.hpp"
#include "library/Track.hpp"

#include <QPainter>
#include <QPixmap>

namespace {
constexpr int kColumns = 5;
constexpr int kIconSize = 20;

QString formatMs(qint64 ms) {
    const qint64 total = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

// Заглушка вместо обложки: мини-винил в цветах плеера.
QIcon makeFallbackIcon() {
    QPixmap pixmap(kIconSize, kIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(79, 140, 255));
    painter.drawEllipse(1, 1, kIconSize - 2, kIconSize - 2);
    painter.setBrush(QColor(18, 18, 18));
    painter.drawEllipse(5, 5, 10, 10);
    painter.setBrush(QColor(157, 93, 255));
    painter.drawEllipse(9, 9, 2, 2);
    return QIcon(pixmap);
}
}  // namespace

LibraryModel::LibraryModel(MusicLibrary& library, QObject* parent)
    : QAbstractTableModel(parent), library_(library) {
    connect(&library_, &MusicLibrary::libraryChanged, this, [this] {
        iconCache_.clear();
        beginResetModel();
        endResetModel();
    });
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(library_.tracks().size());
}

int LibraryModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : kColumns;
}

QIcon LibraryModel::iconFor(const Track& track) const {
    const QString key = QString::fromStdString(track.path.string());
    const auto it = iconCache_.constFind(key);
    if (it != iconCache_.cend()) return it.value();

    QIcon icon;
    QPixmap pixmap;
    if (!track.coverData.isEmpty() && pixmap.loadFromData(track.coverData)) {
        pixmap = pixmap.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
        icon = QIcon(pixmap);
    } else {
        icon = makeFallbackIcon();
    }
    iconCache_.insert(key, icon);
    return icon;
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};

    const auto& tracks = library_.tracks();
    if (index.row() < 0 || index.row() >= static_cast<int>(tracks.size())) return {};
    const Track& track = tracks[static_cast<std::size_t>(index.row())];

    if (role == Qt::DecorationRole && index.column() == 1) return iconFor(track);

    if (role == Qt::TextAlignmentRole && (index.column() == 0 || index.column() == 4)) {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return index.row() + 1;
        case 1:
            return track.title.isEmpty() ? QString::fromStdString(track.path.filename().string())
                                         : track.title;
        case 2:
            return track.artist.isEmpty() ? QStringLiteral("—") : track.artist;
        case 3:
            return track.album.isEmpty() ? QStringLiteral("—") : track.album;
        case 4:
            return formatMs(track.durationMs);
        }
    }
    return {};
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
    switch (section) {
    case 0:
        return QStringLiteral("#");
    case 1:
        return tr("Название");
    case 2:
        return tr("Исполнитель");
    case 3:
        return tr("Альбом");
    case 4:
        return tr("Длительность");
    }
    return {};
}
