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
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <filesystem>

namespace {
constexpr auto kFolderKey = "library/folder";
}

LibraryPanel::LibraryPanel(MusicLibrary& library, QWidget* parent)
    : QWidget(parent), library_(library) {
    auto* openBtn = new QPushButton(tr("Открыть папку…"), this);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Поиск по названию…"));
    search_->setClearButtonEnabled(true);

    folderLabel_ = new QLabel(tr("Папка не выбрана"), this);
    folderLabel_->setWordWrap(false);

    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    top->addWidget(openBtn);
    top->addWidget(search_, 1);

    model_ = new LibraryModel(library_, this);
    proxy_ = new QSortFilterProxyModel(this);
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
    view_->sortByColumn(1, Qt::AscendingOrder);
    view_->hideColumn(0);  // нумерация съедает ширину — места не хватает на названия

    // # и текстовые колонки — по содержимому, «Название» занимает свободное
    auto* header = view_->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    layout->addLayout(top);
    layout->addWidget(folderLabel_);
    layout->addWidget(view_, 1);

    connect(openBtn, &QPushButton::clicked, this, &LibraryPanel::chooseFolder);
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
