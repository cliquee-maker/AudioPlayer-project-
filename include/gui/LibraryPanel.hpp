#pragma once

#include <QWidget>

class LibraryModel;
class MusicLibrary;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;

class LibraryPanel : public QWidget {
    Q_OBJECT
public:
    explicit LibraryPanel(MusicLibrary& library, QWidget* parent = nullptr);

public slots:
    void chooseFolder();

signals:
    void trackActivated(int sourceRow);  // двойной клик/Enter по треку

private:
    MusicLibrary& library_;
    LibraryModel* model_;
    QSortFilterProxyModel* proxy_;
    QTableView* view_;
    QLabel* folderLabel_;
    QLineEdit* search_;
};
