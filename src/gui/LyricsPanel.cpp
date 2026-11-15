#include "gui/LyricsPanel.hpp"

#include <QAction>
#include <QContextMenuEvent>
#include <QFrame>
#include <QMenu>
#include <QTextEdit>
#include <QVBoxLayout>

#include <functional>

namespace {

// QTextEdit со стандартным меню, в которое добавлен пункт «Свернуть».
class LyricsTextEdit : public QTextEdit {
public:
    explicit LyricsTextEdit(QWidget* parent = nullptr) : QTextEdit(parent) {}

    std::function<void()> onCollapse;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override {
        QMenu* menu = createStandardContextMenu();
        menu->addSeparator();
        QAction* collapse = menu->addAction(tr("Свернуть"));
        const QAction* chosen = menu->exec(event->globalPos());
        if (chosen == collapse && onCollapse) onCollapse();
        delete menu;
        event->accept();
    }
};

}  // namespace

LyricsPanel::LyricsPanel(QWidget* parent) : QWidget(parent) {
    auto* text = new LyricsTextEdit(this);
    text->onCollapse = [this] { emit collapseRequested(); };
    text_ = text;
    text_->setReadOnly(true);
    text_->setAcceptRichText(false);
    text_->setPlaceholderText(tr("Здесь появится текст песни…"));
    text_->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(text_, 1);
}
