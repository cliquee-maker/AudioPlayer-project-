# 🎵 LOCAL AUDIO PLAYER

**LOCAL AUDIO PLAYER** — это программа для локального прослушивания аудиофайлов с поддержкой библиотеки треков, плейлистов, визуализации звука и настраиваемого эквалайзера.

---

## 🛠 Технологический стек

* **Язык программирования:** C++
* **Графика и звук:** Qt 6 (Qt Widgets) — рендеринг интерфейса, обработка ввода, окна и диалоги.
* **Пользовательский интерфейс (GUI):** Qt Widgets (Qt Designer для разметки `.ui`, модель/представление для списков) — окна плеера, библиотеки, плейлистов и настроек.
* **Воспроизведение аудио:** Qt Multimedia — `QMediaPlayer` + `QAudioOutput` (play/pause/stop/seek/громкость/состояние).
* **Метаданные аудио:** TagLib — чтение тегов (исполнитель, альбом, обложка, длительность) из файлов.
* **Сериализация данных:** nlohmann/json — сохранение и загрузка плейлистов и настроек в формате `.plst` (JSON).
* **Сборка проекта:** CMake (Qt6 + `AUTOMOC`/`AUTOUIC`/`AUTORCC`).

---

## 🚀 Основные возможности

### 📚 Библиотека и файлы
* **Сканирование локальных папок:** Автоматическое добавление аудиофайлов в библиотеку по указанному пути.
* **Поддерживаемые форматы:** MP3, WAV, OGG, FLAC.
* **Чтение метаданных:** Отображение исполнителя, альбома, обложки и длительности трека.
* **Поиск и сортировка:** Быстрый поиск по названию/исполнителю, сортировка по различным параметрам.

### 🎧 Плеер
* **Базовое управление:** Воспроизведение, пауза, перемотка, следующий/предыдущий трек.
* **Регулировка громкости и эквалайзер:** Настройка нескольких полос частот.
* **Режимы воспроизведения:** Повтор трека/плейлиста, перемешивание (shuffle).
* **Визуализация звука:** Спектр-анализатор и осциллограф в реальном времени (собственный `QWidget` с `QPainter`).

### 📝 Плейлисты
* **Создание и редактирование:** Добавление и удаление треков, изменение порядка.
* **Кастомный формат файлов (`.plst`):** Сохранение и загрузка плейлистов вместе с путями к трекам.
* **Импорт/экспорт:** Обмен плейлистами между пользователями.

### ⌨️ Дополнительно
* **Горячие клавиши:** Управление плеером без обращения к интерфейсу.
* **Мини-режим:** Компактное окно плеера поверх других окон (`Qt::Tool` + `Qt::WindowStaysOnTopHint`).

---

##🛠 Сборка на Linux и Windows

`CMakeLists.txt` одинаковый для обеих ОС: зависимости берутся из системных пакетов
(Linux) или из **vcpkg** (Windows), а если их нет — достраиваются из исходников
через `FetchContent` (`-DAUDIOPLAYER_FETCH_DEPS=ON`, включено по умолчанию).

### Linux (Arch / Manjaro)

```bash
sudo pacman -S --needed base-devel cmake qt6-base taglib nlohmann-json
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/app
```

### Windows (MSVC + vcpkg + Qt)

1. Установить [vcpkg](https://vcpkg.io) и Qt 6 через
   [aqtinstall](https://github.com/microsoft/aqtinstall) (единственный способ,
   работающий одинаково на Windows и Linux):

   ```powershell
   pip install aqtinstall
   pip install pyside6-essentials   # необязательно, только для aqt
   aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtmultimedia
   ```
   Путь к установленному Qt (`...\6.8.3\msvc2022_64`) сохраните в переменную
   окружения `QT_ROOT_DIR`.

2. Установить зависимости через vcpkg (файл `vcpkg.json` уже в проекте):

   ```powershell
   set VCPKG_ROOT=C:\vcpkg
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg install            # подтянет qt6-base, taglib, nlohmann-json
   ```

3. Сконфигурировать и собрать (MSVC: «Developer PowerShell for VS 2022»):

   ```powershell
   cmake -S . -B build -G Ninja ^
     -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
     -DCMAKE_PREFIX_PATH=$env:QT_ROOT_DIR ^
     -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build build
   ```

   Либо готовым пресетом: `cmake --preset windows-msvc && cmake --build --preset windows-msvc`.

4. Чтобы DLL Qt рядом с `.exe`:

   ```powershell
   & $env:QT_ROOT_DIR\bin\windeployqt.exe build\app.exe
   ```

### Пресеты CMake

```bash
cmake --preset linux-system   && cmake --build --preset linux-system
cmake --preset windows-msvc   && cmake --build --preset windows-msvc
```

### Правила кода, чтобы проект собирался на обеих ОС

* Пути к файлам — через `QDir`/`QFileInfo`/`std::filesystem`, а не через `/` в строках.
* Ассеты (обложки, пресеты) ищем через `AUDIOPLAYER_ASSETS_DIR` (макрос из CMake),
  а не относительно текущего каталога.
* Никаких Linux-only вызовов (`unistd.h`, `pthread` в коде) без `#ifdef Q_OS_WIN`.
* Разделитель путей — `QDir::separator()` / `std::filesystem::path`.

---

## 👥 Распределение задач и зон ответственности

> Замените заглушки на реальные имена — зоны ответственности разбиты по фактическим модулям проекта (`core/`, `audio/`, `library/`, `gui/`).

* **Никита Рудинский: Архитектура, главный цикл и аудиодвижок**
  * Реализация класса `App`: создание `QApplication`/`QMainWindow`, главный цикл приложения (`App::run`), обработка событий.
  * Разработка `AudioEngine` — обёртки над `QMediaPlayer`/`QAudioOutput`: play/pause/stop, громкость, перемотка (`seek`), отслеживание состояния воспроизведения через сигналы.
  * Логика автоперехода к следующему треку по завершении текущего.
  * Настройка сборки: `CMakeLists.txt`, `find_package(Qt6 COMPONENTS Widgets Multimedia)`, `nlohmann/json`, `AUTOMOC/AUTOUIC/AUTORCC`.

* **Лукашевич Арсений: Графический интерфейс и визуализация, маркетинг**
  * Разработка интерфейса на Qt Widgets: главное окно (`QMainWindow`), панель библиотеки, панель плейлиста, панель управления (play/пауза/стоп/громкость/перемотка).
  * Реализация модуля `Visualizer` — собственный виджет на `QPainter`, реагирующий на громкость и время воспроизведения.
  * Стилизация приложения: QSS-тема (`.qss`), единый визуальный стиль.

* **Маскалик Алексей: Библиотека треков, плейлисты и тестирование, базы данных**
  * Разработка `MusicLibrary`: рекурсивное сканирование папки, определение поддерживаемых форматов (`.mp3`, `.ogg`, `.wav`, `.flac`).
  * Реализация `Track` и `Playlist`: добавление/удаление/перемещение треков, сериализация и загрузка в формате `.plst` (JSON) через `nlohmann/json`.
  * Написание smoke-тестов (`tests/test_playlist.cpp`) и проверка сборки/тестов через `ctest`.
  * Дальнейшее расширение метаданных (интеграция TagLib для тегов и обложек) и поддержки MP3.

---

## 📂 Структура проекта

```text
audio-player/
├── assets/
│   └── music/                 # Сюда кладутся аудиофайлы (.mp3/.ogg/.wav/.flac)
├── include/                   # Заголовочные файлы (.hpp)
│   ├── core/                   # App — окно, главный цикл, состояние
│   ├── audio/                  # AudioEngine — обёртка над QMediaPlayer
│   ├── library/                 # Track, MusicLibrary (сканирование), Playlist (JSON)
│   └── gui/                     # Visualizer и компоненты интерфейса
├── src/                        # Реализации (.cpp) + main.cpp
├── tests/                      # test_playlist.cpp — smoke-тест
├── CMakeLists.txt              # Скрипт сборки CMake (Qt6, nlohmann/json)
└── README.md
```
