#include "gui/LibraryPanel.hpp"

#include "gui/LibraryModel.hpp"
#include "library/MusicLibrary.hpp"

#include <QAbstractItemView>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <filesystem>
#include <limits>

namespace {
constexpr auto kFolderKey = "library/folder";

// Сортировка альбомов по метаданным: альбом, затем номер трека.
// Qt при Z→A вызывает lessThan с переставленными аргументами, поэтому
// номер трека сравниваем в обратную сторону — внутри альбома он всегда
// остаётся по возрастанию (1, 2, 3, ...).
class AlbumSortProxyModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
        if (left.column() != 3 || right.column() != 3)
            return QSortFilterProxyModel::lessThan(left, right);

        const bool descending = sortOrder() == Qt::DescendingOrder;

        const int albumCmp = QString::compare(left.data(Qt::DisplayRole).toString(),
                                              right.data(Qt::DisplayRole).toString(),
                                              Qt::CaseInsensitive);
        if (albumCmp != 0) return albumCmp < 0;

        constexpr int kUnknownTrack = std::numeric_limits<int>::max();
        const int leftNumber = left.data(LibraryModel::TrackNumberRole).toInt();
        const int rightNumber = right.data(LibraryModel::TrackNumberRole).toInt();
        const int leftRank = leftNumber == 0 ? kUnknownTrack : leftNumber;
        const int rightRank = rightNumber == 0 ? kUnknownTrack : rightNumber;
        if (leftRank != rightRank) return descending ? leftRank > rightRank : leftRank < rightRank;

        const int artistCmp = QString::compare(
            left.siblingAtColumn(2).data(Qt::DisplayRole).toString(),
            right.siblingAtColumn(2).data(Qt::DisplayRole).toString(), Qt::CaseInsensitive);
        if (artistCmp != 0) return descending ? artistCmp > 0 : artistCmp < 0;

        const int titleCmp = QString::compare(
            left.siblingAtColumn(1).data(Qt::DisplayRole).toString(),
            right.siblingAtColumn(1).data(Qt::DisplayRole).toString(), Qt::CaseInsensitive);
        if (titleCmp != 0) return descending ? titleCmp > 0 : titleCmp < 0;

        return QSortFilterProxyModel::lessThan(left, right);
    }
};
}  // namespace

LibraryPanel::LibraryPanel(MusicLibrary& library, QWidget* parent)
    : QWidget(parent), library_(library) {
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Поиск по названию…"));
    search_->setClearButtonEnabled(true);

    folderLabel_ = new QLabel(tr("Папка не выбрана"), this);
    folderLabel_->setWordWrap(false);

    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    top->addWidget(search_, 1);

    model_ = new LibraryModel(library_, this);
    proxy_ = new AlbumSortProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setFilterKeyColumn(-1);  // поиск по всем столбцам

    view_ = new QTableView(this);
    view_->setModel(proxy_);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setAlternatingRowColors(true);
    view_->verticalHeader()->setVisible(false);
    view_->verticalHeader()->setDefaultSectionSize(26);
    view_->setIconSize(QSize(20, 20));
    view_->setSortingEnabled(true);
    view_->sortByColumn(3, Qt::AscendingOrder);
    view_->hideColumn(0);  // нумерация съедает ширину — места не хватает на названия

    // # и «Название» — по содержимому/растяжению; исполнитель, альбом и
    // длительность — узкие колонки, чтобы название получало максимум ширины
    auto* header = view_->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Interactive);
    header->setSectionResizeMode(3, QHeaderView::Interactive);
    header->setSectionResizeMode(4, QHeaderView::Interactive);
    header->resizeSection(2, 110);
    header->resizeSection(3, 100);
    header->resizeSection(4, 64);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    layout->addLayout(top);
    layout->addWidget(folderLabel_);
    layout->addWidget(view_, 1);

    connect(search_, &QLineEdit::textChanged, proxy_,
            &QSortFilterProxyModel::setFilterFixedString);
    connect(view_, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        emit trackActivated(proxy_->mapToSource(index).row());
    });

    // Восстановление последней папки библиотеки
    QSettings settings;
    const QString saved = settings.value(kFolderKey).toString();
    if (!saved.isEmpty() &&
        std::filesystem::is_directory(std::filesystem::u8path(saved.toStdString()))) {
        folderLabel_->setText(saved);
        library_.scan(std::filesystem::u8path(saved.toStdString()));
    } else if (!library_.tracks().empty()) {
        folderLabel_->setText(tr("Треков в библиотеке: %1")
                                  .arg(static_cast<qint64>(library_.tracks().size())));
    }
}

void LibraryPanel::chooseFolder() {
    QSettings settings;
    const QString saved = settings.value(kFolderKey).toString();
    const QString start = saved.isEmpty() ? QDir::homePath() : saved;

    // Нативный системный проводник (в Qt — штатный QFileDialog)
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Выберите папку с музыкой"),
                                                          start, QFileDialog::ShowDirsOnly);
    if (dir.isEmpty()) return;

    settings.setValue(kFolderKey, dir);
    folderLabel_->setText(dir);
    library_.scan(std::filesystem::u8path(dir.toStdString()));
}
