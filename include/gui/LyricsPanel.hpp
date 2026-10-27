#pragma once

#include <QWidget>

class QTextEdit;

// Левая верхняя область: место под текст песни (lyrics).
class LyricsPanel : public QWidget {
    Q_OBJECT
public:
    explicit LyricsPanel(QWidget* parent = nullptr);

private:
    QTextEdit* text_;
};
