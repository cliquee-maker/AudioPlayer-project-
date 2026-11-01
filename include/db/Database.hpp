#pragma once

// ============================================================================
// Слой БД (Этап 1) — Database
// Единственное место, где создаётся QSqlDatabase-соединение: открытие файла
// SQLite, PRAGMA-настройки, миграции схемы и транзакции.
//
// Правило потоков: одно соединение — один поток. Для фонового скана создаём
// отдельный Database с другим connectionName (см. DB_API_PLAN.md, 4.4).
// ============================================================================

#include <QSqlDatabase>
#include <QString>

class Database {
public:
    explicit Database(QString connectionName = QStringLiteral("audioplayer"));
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Открывает (при необходимости создаёт) файл БД и включает PRAGMA.
    bool open(const QString& filePath = defaultPath());
    bool isOpen() const;

    // Создаёт/обновляет схему до актуальной версии. Идемпотентна.
    bool migrate();

    QSqlDatabase handle() const;

    // Транзакции: массовый скан/импорт истории одной транзакцией.
    bool beginTransaction();
    bool commit();
    bool rollback();

    // Версия схемы из PRAGMA user_version (-1, если БД не открыта).
    int schemaVersion() const;

    // AppDataLocation/audioplayer.db — не текущий каталог.
    static QString defaultPath();

    // Кэш обложек рядом с БД: AppDataLocation/covers.
    static QString coversDir();

private:
    bool applyPragmas();
    bool migrateToV1();

    QString connectionName_;
};
