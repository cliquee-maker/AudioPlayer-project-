# План: GUI для LOCAL AUDIO PLAYER

**Задачи:** Лукашевич Арсений — графический интерфейс и визуализация.
**Контекст:** репозиторий пуст (только README, ветка `Structure_Creation`). GUI пишется против согласованных заголовков-контрактов. Визуализатор — имитация по времени/громкости с абстракцией под будущий FFT.

---

## Этап 0 — Согласование API-контрактов с командой (коммит: заголовки в `include/`)

До написания GUI зафиксировать минимальные чистые заголовки, которые Арсений будет использовать. Это ключевой коммуникационный шаг.

**`include/audio/AudioEngine.hpp`** (ждём от Никиты; Арсений кодит против этого):

```cpp
class AudioEngine {
public:
    bool open(const std::filesystem::path& file);
    void play(); void pause(); void stop();
    void seek(sf::Time t);
    void setVolume(float v);          // 0..100
    float volume() const;
    bool isPlaying() const;
    sf::Time currentTime() const;
    sf::Time duration() const;
    const std::vector<float>& spectrum() const;  // сейчас: пусто/симуляция; позже — FFT (см. этап 4)
};
```

> **Важно обсудить на команде:** `spectrum()` — Арсений проектирует визуализатор так, чтобы подменить источник данных без переделки UI (см. этап 4).

**`include/library/Track.hpp`** (ждём от Алексея):

```cpp
struct Track {
    std::filesystem::path path;
    std::string title, artist, album;
    sf::Time duration;
    std::vector<uint8_t> coverData;   // JPEG/PNG из TagLib → Арсений грузит в sf::Texture
};
```

**`include/core/App.hpp`** (ждём от Никиты):

```cpp
class App {
public:
    int run();
    sf::RenderWindow& window();
    AudioEngine& engine();
    MusicLibrary& library();
    Playlist& playlist();
private:
    void handleEvent(const sf::Event&);
    // хук, который App вызывает каждый кадр между ImGui::SFML::Update и Render:
    void drawGUI();
};
```

Договорённость по кадру:
`pollEvent → ImGui::SFML::ProcessEvent → ... → ImGui::SFML::Update(window, dt) → drawGUI() → ImGui::SFML::Render(window)`.

---

## Этап 1 — CMake и зависимости (делает Никита; Арсений готовит сниппет и проверяет)

- `find_package(SFML 2.6 COMPONENTS graphics window system audio REQUIRED)` (или FetchContent).
- `FetchContent` для `imgui-sfml` (v2.6.x, привязан к SFML 2.6) с `IMGUI_SFML_IMGUI_DEMO=OFF`; он сам тянет ImGui.
- Цели: `nlohmann_json`, `taglib`.
- Библиотека `gui` (static): `src/gui/*.cpp` → линкуется в `app`.
- Арсений: поднять локально «демо-сборку» с одним пустым ImGui-окном как smoke-check этапа 1 (`cmake -B build && cmake --build build`).

---

## Этап 2 — Интеграция ImGui + SFML (Арсений, в `App.cpp` совместно с Никитой)

Файлы: `src/main.cpp`, `src/core/App.cpp`.

1. `ImGui::SFML::Init(window)` после создания окна (стиль `ImGui::StyleColorsDark()`).
2. В цикле: `ImGui::SFML::ProcessEvent`, `ImGui::SFML::Update(window, clock.restart())`.
3. `drawGUI()` — вызов всех панелей; затем `ImGui::SFML::Render(window)`.
4. Шрифт: подключить DejaVuSans/Roboto + кириллица (язык интерфейса — уточнить), `io.Fonts->AddFontFromFileTTF`, `ImGui::SFML::UpdateFontTexture()`.

---

## Этап 3 — Панели интерфейса (`include/gui/`, `src/gui/`)

Каждая панель — класс с `void draw(...)` и конструктором, принимающим ссылки на `AudioEngine/MusicLibrary/Playlist`.

| Файл | Содержимое |
|---|---|
| `gui/ControlPanel.hpp/.cpp` | Кнопки ⏮ ⏯ ⏭ ⏹, `ImGui::SliderFloat` громкости, `ImGui::SliderFloat`/`ProgressBar` перемотки (перетаскивание → `engine.seek()`), счётчик времени `mm:ss / mm:ss`, режимы repeat/shuffle (кнопки-флаги), отображение текущего трека (title — artist) |
| `gui/LibraryPanel.hpp/.cpp` | Кнопка «Сканировать папку» (диалог пути — см. примечание ниже), `ImGui::InputText` поиск по title/artist, `ImGui::BeginChild` + `ImGui::Selectable`-таблица треков (столбцы: #, Название, Исполнитель, Альбом, Длительность), сортировка по клику на заголовок (`ImGui::TableSetupSortSpecs`), dbl-click → play, контекстное меню «Добавить в плейлист» |
| `gui/PlaylistPanel.hpp/.cpp` | Текущий плейлист: список с drag-and-drop reorder (`ImGui::BeginDragDropSource/Target`), кнопки +/−/⬆⬇, «Сохранить/Загрузить .plst» (через API Алексея), экспорт/импорт |
| `gui/SettingsPanel.hpp/.cpp` | Эквалайзер (несколько `ImGui::SliderFloat` по полосам → `engine` API), горячие клавиши (справочная таблица), тема (светлая/тёмная), настройки папки библиотеки (сохранение в `settings.json`) |
| `gui/CoverArt.hpp/.cpp` | Загрузка `coverData` в `sf::Texture` (лениво, по ID трека), отрисовка `ImGui::Image` |

**Примечание по диалогам выбора папки:** нативных диалогов в SFML/ImGui нет — либо простой `ImGui::InputText` для пути (рекомендую, минимум зависимостей), либо `ImGuiFileDialog` через FetchContent (решение команде).

**Мини-режим:** флаг в `App` — компактное окно ~320×90 (кнопки + ползунок + визуализатор), `sf::Style::None` + always-on-top не поддерживается SFML 2.6 → реализуем как маленький layout внутри того же окна, либо второе окно (уточнить с Никитой, кто владеет окном).

**Горячие клавиши:** обработка в `App::handleEvent` (зона Никиты) → Арсений предоставляет карту действий и справку в SettingsPanel.

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
    virtual sf::Time position() const = 0;
};
```

- **Сейчас** — `SimulatedVisualSource`: на основе `position()/volume()/isPlaying()` детерминированный псевдошум (сумма синусов с разными частотами + decay при паузе), плавность через `ImGui::GetIO().DeltaTime` и экспоненциальное сглаживание (attack/release) — полосы «дышат».
- **Реализация:** `Visualizer::draw(ImDrawList*, ImVec2 pos, ImVec2 size, Mode mode)`, `Mode { Bars, Oscilloscope }`.
  - **Bars:** N=48–64 прямоугольников, градиент от `accent` → `secondary`, сглаженная высота, после спада белая «вершина-пики» (peak hold с таймером).
  - **Oscilloscope:** `PathLineStrip` из точек синусоидоподобной волны, толщина линии 2px, glow-эффект (полупрозрачная копия с большим offset).
- Реальный FFT (позже): тот же `bins()` заполняется из `AudioEngine::spectrum()` — UI не меняется.

---

## Этап 5 — Стилизация (общий визуальный стиль)

- `gui/Theme.hpp/.cpp`: функция `applyDarkTheme()` — цвета `ImGuiCol_*` (фон `#121212`, панели `#1E1E1E`, акцент — уточнить бренд), `ImGui::GetStyle().WindowRounding = 8.f`, паддинги, шрифт 16px + жирный для заголовков.
- Единый стиль для всех окон; кастомные кнопки через `PushStyleColor` (метод `styledButton(label, size)`).
- Кадрирование обложек `ImGui::Image` с сохранением пропорций.

---

## Этап 6 — Проверка и маркетинг (Арсений)

- Сборка: `cmake -B build && cmake --build build`, визуальный прогон со сканом `assets/music/`.
- Скриншоты/GIF плеера, визуализатора и плейлистов для презентации проекта (маркетинг-задача).
- Проверка, что `ctest` коллег не сломан.

---

## Порядок и зависимости

```
Этап 0 (контракты) ──► Этап 1 (CMake) ──► Этап 2 (интеграция ImGui)
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

## Ключевые риски

1. **Отсутствие PCM** → визуализатор-имитация (согласовано); держать интерфейс через `IVisualSource`.
2. **Диалоги выбора файлов** — решить: `InputText` или `ImGuiFileDialog`.
3. **Кириллица/шрифты** — подключить TTF с кириллицей явно.
4. **Синхронизация заголовков** — контракты этапа 0 должны утвердить все трое; изменения API ломают `gui`.
