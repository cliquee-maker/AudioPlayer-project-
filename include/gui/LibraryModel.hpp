#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>

class MusicLibrary;
struct Track;

class LibraryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit LibraryModel(MusicLibrary& library, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QIcon iconFor(const Track& track) const;

    MusicLibrary& library_;
    mutable QHash<QString, QIcon> iconCache_;
};
