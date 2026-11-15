#include "gui/RecommendationsPanel.hpp"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Статичные папки-заглушки. Названия и количества треков — макет;
// наполнение появится, когда будет готов модуль рекомендаций.
struct FolderStub {
    const char* title;
    const char* hint;
};

const FolderStub kFolders[] = {
    {"Похожее на любимое", "15 треков"},
    {"Любимые жанры", "8 треков"},
    {"Для вечера", "12 треков"},
    {"Новое из библиотеки", "24 трека"},
};

QToolButton* makeFolderCard(const QString& title, const QString& hint, QWidget* parent) {
    auto* card = new QToolButton(parent);
    card->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    card->setIcon(card->style()->standardIcon(QStyle::SP_DirIcon));
    card->setIconSize(QSize(48, 48));
    card->setText(title + QLatin1Char('\n') + hint);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setMinimumHeight(104);
    card->setCursor(Qt::PointingHandCursor);
    card->setToolTip(QObject::tr("Папка-заглушка: содержимое появится позже"));
    return card;
}

}  // namespace

RecommendationsPanel::RecommendationsPanel(QWidget* parent) : QWidget(parent) {
    auto* title = new QLabel(tr("Рекомендации"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);

    auto* backButton = new QPushButton(tr("К каталогу"), this);
    backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    connect(backButton, &QPushButton::clicked, this, &RecommendationsPanel::backRequested);

    auto* header = new QHBoxLayout;
    header->setSpacing(8);
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(backButton);

    auto* subtitle = new QLabel(
        tr("Папки собраны по вашим локальным файлам"), this);
    subtitle->setStyleSheet(QStringLiteral("color: gray;"));

    statusLabel_ = new QLabel(
        tr("Папки-заглушки: наполнение появится после подключения рекомендаций."),
        this);
    statusLabel_->setStyleSheet(QStringLiteral("color: gray;"));
    statusLabel_->setWordWrap(true);

    auto* folderRow = new QHBoxLayout;
    folderRow->setSpacing(8);
    for (const FolderStub& stub : kFolders) {
        auto* card = makeFolderCard(tr(stub.title), tr(stub.hint), this);
        connect(card, &QToolButton::clicked, this, [this, folderTitle = tr(stub.title)] {
            statusLabel_->setText(
                tr("Заглушка: папка «%1» пока не открывается — подключим вместе "
                   "с логикой рекомендаций.")
                    .arg(folderTitle));
        });
        folderRow->addWidget(card, 1);
    }

    auto* apiCaption = new QLabel(tr("Рекомендации из API"), this);
    apiCaption->setObjectName(QStringLiteral("recommendationsCaption"));

    apiText_ = new QTextEdit(this);
    apiText_->setReadOnly(true);
    apiText_->setAcceptRichText(false);
    apiText_->setFrameShape(QFrame::StyledPanel);
    apiText_->setPlainText(
        tr("Здесь появятся текстовые рекомендации, полученные из API.\n\n"
           "Пока это заглушка. Пример будущего ответа:\n\n"
           "• «Вечерний джаз» — подборка на основе ваших локальных треков.\n"
           "• «Похоже на …» — исполнители, которые уже есть в библиотеке.\n"
           "• «Открытия недели» — новые имена в ваших любимых жанрах.\n\n"
           "(Текст и папки заполняются модулем рекомендаций.)"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    layout->addLayout(header);
    layout->addWidget(subtitle);
    layout->addLayout(folderRow);
    layout->addWidget(statusLabel_);
    layout->addWidget(apiCaption);
    layout->addWidget(apiText_, 1);
}
