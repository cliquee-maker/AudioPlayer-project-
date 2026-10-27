#include "gui/LyricsPanel.hpp"

#include <QFrame>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

LyricsPanel::LyricsPanel(QWidget* parent) : QWidget(parent) {
    auto* caption = new QLabel(tr("Текст песни"), this);
    caption->setObjectName(QStringLiteral("lyricsCaption"));

    text_ = new QTextEdit(this);
    text_->setReadOnly(true);
    text_->setAcceptRichText(false);
    text_->setPlaceholderText(tr("Здесь появится текст песни…"));
    text_->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(caption);
    layout->addWidget(text_, 1);
}
