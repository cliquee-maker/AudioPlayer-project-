# План: GUI для LOCAL AUDIO PLAYER

**Задачи:** Лукашевич Арсений — графический интерфейс и визуализация.
**Контекст:** проект переводится со связки **SFML + Dear ImGui** на **Qt 6 (Qt Widgets + Qt Multimedia)**. GUI пишется против согласованных заголовков-контрактов. Визуализатор — имитация по времени/громкости с абстракцией под будущий FFT.

---

## Этап 0 — Согласование API-контрактов с командой (коммит: заголовки в `include/`)

До написания GUI зафиксировать минимальные чистые заголовки, которые Арсений будет использовать. Это ключевой коммуникационный шаг.

**`include/audio/AudioEngine.hpp`** (ждём от Никиты; Арсений кодит против этого):

```cpp
class AudioEngine : public QObject {
    Q_OBJECT
public:
    bool open(const std::filesystem::path& file);
    void play(); void pause(); void stop();
    void seek(qint64 positionMs);      // аналог старого seek(sf::Time)
    void setVolume(float v);           // 0..100
    float volume() const;
    bool isPlaying() const;
    qint64 positionMs() const;        // аналог currentTime()
    qint64 durationMs() const;        // аналог duration()
    const std::vector<float>& spectrum() const;  // сейчас: пусто/симуляция; позже — FFT (см. этап 4)

signals:
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void stateChanged(bool playing);
    void mediaFinished();
};
```

> **Важно обсудить на команде:** `spectrum()` — Арсений проектирует визуализатор так, чтобы подменить источник данных без переделки UI (см. этап 4). Тайминги в Qt — целые миллисекунды (`qint64`), а не `sf::Time`.

**`include/library/Track.hpp`** (ждём от Алексея):

```cpp
struct Track {
    std::filesystem::path path;
    QString title, artist, album;
    qint64 durationMs;
    QByteArray coverData;   // JPEG/PNG из TagLib → Арсений грузит в QPixmap
};
```

**`include/core/App.hpp`** (ждём от Никиты):

```cpp
class App {
public:
    int run();                  // создаёт QApplication, главное окно, запускает event loop
    QMainWindow& window();
    AudioEngine& engine();
    MusicLibrary& library();
    Playlist& playlist();
private:
    // хук, который App вызывает каждый кадр (через QTimer ~60 Гц) в основном окне:
    void drawGUI();
};
```

Договорённость по кадру:
`QCoreApplication::processEvents → обновление состояния движка → drawGUI() (виджеты/QPainter) → отрисовка окна средствами Qt`.
В Qt нет ручного цикла `pollEvent → Update → Render`: цикл событий и перерисовка управляются самой Qt, а анимация визуализатора крутится через `QTimer` (шаг 16 мс).

---

## Этап 1 — CMake и зависимости (делает Никита; Арсений готовит сниппет и проверяет)

- `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)`.
- Включить `set(CMAKE_AUTOMOC ON)`, `set(CMAKE_AUTOUIC ON)`, `set(CMAKE_AUTORCC ON)`.
- Цели: `nlohmann_json`, `taglib`.
- Библиотека `gui` (static): `src/gui/*.cpp` → линкуется в `app`.
- Qt 6 ставится из пакетного менеджера (Arch: `qt6-base`, `qt6-multimedia`) — зависимости подключаются через `find_package`, без `FetchContent`.
- Арсений: поднять локально «демо-сборку» с одним пустым `QMainWindow` как smoke-check этапа 1 (`cmake -B build && cmake --build build`).

---

## Этап 2 — Интеграция Qt (Арсений, в `App.cpp` совместно с Никитой)

Файлы: `src/main.cpp`, `src/core/App.cpp`.

1. `main()`: создать `QApplication` (для HiDPI — `Qt::AA_EnableHighDpiScaling`), создать `App`, вызвать `App::run()`.
2. Главное окно: `QMainWindow` с центральным виджетом и `QDockWidget`-панелями (библиотека / плейлист / настройки) — переключение видимости через `QAction` в меню.
3. Тема: единый `QSS`-файл (`theme.qss`) + палитра `QPalette`; тёмная/светлая тема переключается в настройках.
4. Шрифт: подключить DejaVuSans/Roboto + кириллица (язык интерфейса — уточнить) через `QFontDatabase::addApplicationFont`.

---

## Этап 3 — Панели интерфейса (`include/gui/`, `src/gui/`)

Каждая панель — класс-наследник `QWidget` (или `QDockWidget`) с конструктором, принимающим ссылки на `AudioEngine/MusicLibrary/Playlist`.

| Файл | Содержимое |
|---|---|
| `gui/ControlPanel.hpp/.cpp` | Кнопки ⏮ ⏯ ⏭ ⏹ (`QToolButton`), `QSlider` громкости, `QSlider` перемотки (перетаскивание → `engine.seek()`), счётчик времени `mm:ss / mm:ss`, режимы repeat/shuffle (чекбоксы), отображение текущего трека (title — artist) |
| `gui/LibraryPanel.hpp/.cpp` | Кнопка «Сканировать папку» (диалог пути — см. примечание ниже), `QLineEdit` поиск по title/artist, `QTableView` треков (столбцы: #, Название, Исполнитель, Альбом, Длительность) на базе `QAbstractTableModel` + `QSortFilterProxyModel` (сортировка по клику на заголовок, фильтр по поиску), dbl-click → play, контекстное меню «Добавить в плейлист» |
| `gui/PlaylistPanel.hpp/.cpp` | Текущий плейлист: `QListWidget` с drag-and-drop reorder (встроенный `InternalMove`), кнопки +/−/⬆⬇, «Сохранить/Загрузить .plst» (через API Алексея), экспорт/импорт |
| `gui/SettingsPanel.hpp/.cpp` | Эквалайзер (несколько `QSlider` по полосам → `engine` API), горячие клавиши (справочная таблица), тема (светлая/тёмная), настройки папки библиотеки (сохранение в `settings.json` / `QSettings`) |
| `gui/CoverArt.hpp/.cpp` | Загрузка `coverData` в `QPixmap` (лениво, по ID трека, кэш), отрисовка в `QLabel` с сохранением пропорций (`Qt::KeepAspectRatio`) |

**Примечание по диалогам выбора папки:** в Qt есть штатный `QFileDialog::getExistingDirectory` (нативный диалог) — отдельная зависимость (ImGuiFileDialog) больше не нужна. Альтернатива — простой `QLineEdit` для пути.

**Мини-режим:** флаг в `App` — компактное окно ~320×90 (кнопки + ползунок + визуализатор), `Qt::Tool | Qt::WindowStaysOnTopHint` (always-on-top поддерживается нативно), либо второе окно (уточнить с Никитой, кто владеет окном).

**Горячие клавиши:** `QShortcut`/`QKeySequence` на виджетах (зона Никиты) → Арсений предоставляет карту действий и справку в SettingsPanel.

| Клавиша | Действие |
|---|---|
| Space | play/pause |
| ← / → | seek ±5 с |
| ↑ / ↓ | громкость |
| N / P | следующий / предыдущий трек |
| M | мини-режим |

---

## Этап 4 — Модуль `Visualizer` (`include/gui/Visualizer.hpp`, `src/gui/Visualizer.cpp`)

Архитектура с подменой источника:

```cpp
struct IVisualSource {
    virtual ~IVisualSource() = default;
    virtual const std::vector<float>& bins() const = 0;  // 0..1, N полос
    virtual bool isPlaying() const = 0;
    virtual float volume() const = 0;
    virtual qint64 positionMs() const = 0;
};
```

- **Сейчас** — `SimulatedVisualSource`: на основе `positionMs()/volume()/isPlaying()` детерминированный псевдошум (сумма синусов с разными частотами + decay при паузе), плавность через дельту `QTimer`/`QElapsedTimer` и экспоненциальное сглаживание (attack/release) — полосы «дышат».
- **Реализация:** `class Visualizer : public QWidget` с `paintEvent(QPaintEvent*)`, отрисовка через `QPainter`; `Mode { Bars, Oscilloscope }`, анимация — `QTimer` (16 мс).
  - **Bars:** N=48–64 прямоугольников, градиент от `accent` → `secondary` (`QLinearGradient`), сглаженная высота, после спада белая «вершина-пики» (peak hold с таймером).
  - **Oscilloscope:** `QPainter::drawPolyline` из точек синусоидоподобной волны, толщина линии 2px, glow-эффект (полупрозрачная копия с большим offset).
- Реальный FFT (позже): тот же `bins()` заполняется из `AudioEngine::spectrum()` — UI не меняется.

---

## Этап 5 — Стилизация (общий визуальный стиль)

- `resources/theme.qss` (Qt Style Sheets) — цвета фона `#121212`, панели `#1E1E1E`, акцент (уточнить бренд), скругления (`border-radius`), паддинги; шрифт 16px + жирный для заголовков.
- Единый стиль для всех окон и доков; кастомные кнопки через объектные селекторы QSS (`QToolButton#playButton`).
- Кадрирование обложек `QLabel` с сохранением пропорций.

---

## Этап 6 — Проверка и маркетинг (Арсений)

- Сборка: `cmake -B build && cmake --build build`, визуальный прогон со сканом `assets/music/`.
- Скриншоты/GIF плеера, визуализатора и плейлистов для презентации проекта (маркетинг-задача).
- Проверка, что `ctest` коллег не сломан.

---

## Порядок и зависимости

```
Этап 0 (контракты) ──► Этап 1 (CMake + Qt6) ──► Этап 2 (каркас QMainWindow)
                                   │
                    ┌──────────────┼──────────────┐
                    ▼              ▼              ▼
              Этап 3.1        Этап 3.2       Этап 4
           ControlPanel     Library/Playlist  Visualizer
                    │              │              │
                    └──────────────┼──────────────┘
                                   ▼
                          Этап 3.3 Settings + Этап 5 (стиль)
                                   ▼
                            Этап 6 (проверка/маркетинг)
```

Параллельно с работой команды над `AudioEngine`/`MusicLibrary`/`Playlist`; GUI сводится на этапах 2–3 после появления первых реализаций.

---

## Миграция SFML/ImGui → Qt (сводная таблица)

| Было (SFML + ImGui) | Стало (Qt 6) |
|---|---|
| `sf::RenderWindow` | `QMainWindow` / `QWidget` |
| `sf::Music` | `QMediaPlayer` + `QAudioOutput` |
| `sf::Time` | `qint64` миллисекунды |
| `ImGui::SFML::Init/Update/Render` | `QApplication` + event loop, `QTimer` для анимации |
| `ImGui::SliderFloat` | `QSlider` / `QDial` |
| `ImGui::TableSetupSortSpecs` | `QAbstractTableModel` + `QSortFilterProxyModel` |
| `ImGui::BeginDragDropSource/Target` | `QListWidget` с `setDragDropMode(InternalMove)` |
| `ImGuiFileDialog` / `InputText` для пути | `QFileDialog` / `QLineEdit` |
| `ImGui::Image` | `QLabel` + `QPixmap` (`KeepAspectRatio`) |
| `ImGui::GetIO().DeltaTime` | дельта из `QElapsedTimer` |
| `ImGuiCol_*` / `StyleColorsDark` | QSS (`theme.qss`) + `QPalette` |
| `sf::Style::None` + always-on-top | `Qt::Tool | Qt::WindowStaysOnTopHint` |

## Ключевые риски

1. **Отсутствие PCM** → визуализатор-имитация (согласовано); держать интерфейс через `IVisualSource`.
2. **Зависимости Qt Multimedia** — в разных дистрибутивах доступны разные бэкенды (FFmpeg/GStreamer); проверить воспроизведение MP3/FLAC на целевой машине.
3. **Кириллица/шрифты** — подключить TTF с кириллицей явно.
4. **Синхронизация заголовков** — контракты этапа 0 должны утвердить все трое; изменения API ломают `gui`.
