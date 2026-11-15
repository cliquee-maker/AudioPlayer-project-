#pragma once

#include <QWidget>

class QLabel;
class QTextEdit;

// Область «Рекомендации»: папки, собранные по локальным файлам, и
// текстовые рекомендации из API. Пока это только интерфейс с заглушками:
// реальные данные и запросы к API подключает отдельный модуль.
class RecommendationsPanel : public QWidget {
    Q_OBJECT
public:
    explicit RecommendationsPanel(QWidget* parent = nullptr);

signals:
    void backRequested();

private:
    QLabel* statusLabel_;
    QTextEdit* apiText_;
};
