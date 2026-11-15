#include "gui/QueuePanel.hpp"

#include <QAction>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

QueuePanel::QueuePanel(QWidget* parent) : QWidget(parent) {
    auto* placeholder = new QLabel(tr("Очередь пуста"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral("color: gray;"));
    placeholder->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(placeholder, &QWidget::customContextMenuRequested, this,
            [this, placeholder](const QPoint& pos) {
                showCollapseMenu(placeholder->mapToGlobal(pos));
            });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(placeholder, 1);
}

void QueuePanel::contextMenuEvent(QContextMenuEvent* event) {
    showCollapseMenu(event->globalPos());
    event->accept();
}

void QueuePanel::showCollapseMenu(const QPoint& globalPos) {
    QMenu menu(this);
    QAction* collapse = menu.addAction(tr("Свернуть"));
    if (menu.exec(globalPos) == collapse) emit collapseRequested();
}
