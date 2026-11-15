#pragma once

#include <QWidget>

class QContextMenuEvent;
class QPoint;

// Вкладка «Очередь» под информацией о треке.
// Пока только заготовка интерфейса: логику очереди реализует Никита.
class QueuePanel : public QWidget {
    Q_OBJECT
public:
    explicit QueuePanel(QWidget* parent = nullptr);

signals:
    void collapseRequested();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void showCollapseMenu(const QPoint& globalPos);
};
