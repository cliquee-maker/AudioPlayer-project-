cmake_minimum_required(VERSION 3.22)
project(AudioPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Общие настройки (одинаково для Linux и Windows) ---
if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus)
    # vcpkg x64-windows(-static): единый рантайм, чтобы не тянуть dll MinGW/VC
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra)
endif()

# Каталог с ассетами: на Windows другая структура папок -> путь задаём через
# макрос, а не вручную в коде.
set(AUDIOPLAYER_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

# --- Qt (Qt Widgets) ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- Зависимости ---
# 1) Пробуем найти установленные пакеты:
#      Linux  -> pacman/apt: qt6-base, taglib, nlohmann-json
#      Windows-> vcpkg: qt6-base, taglib, nlohmann-json
# 2) Если не нашли и AUDIOPLAYER_FETCH_DEPS=ON -> собираем из исходников
#    (FetchContent), чтобы другу на Windows хватило только CMake + компилятора.
option(AUDIOPLAYER_FETCH_DEPS "Собирать отсутствующие зависимости из исходников" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)
find_package(nlohmann_json QUIET)
find_package(taglib QUIET)

# --- nlohmann/json (header-only) ---
if(NOT nlohmann_json_FOUND AND AUDIOPLAYER_FETCH_DEPS)
    message(STATUS "nlohmann_json не найден -> FetchContent")
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
        GIT_SHALLOW    TRUE)
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
    set(nlohmann_json_FOUND TRUE)
endif()

# --- TagLib ---
if(NOT taglib_FOUND AND AUDIOPLAYER_FETCH_DEPS)
    message(STATUS "taglib не найден -> FetchContent")
    include(FetchContent)
    # TagLib требует zlib
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
        FetchContent_Declare(zlib
            GIT_REPOSITORY https://github.com/madler/zlib.git
            GIT_TAG        v1.3.1
            GIT_SHALLOW    TRUE)
        set(ZLIB_BUILD_EXAMPLES OFF CACHE INTERNAL "")
        FetchContent_MakeAvailable(zlib)
        if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        endif()
    endif()
    FetchContent_Declare(taglib
        GIT_REPOSITORY https://github.com/taglib/taglib.git
        GIT_TAG        v2.3.2
        GIT_SHALLOW    TRUE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(TAGLIB_WITH_ASF OFF CACHE BOOL "" FORCE)
    set(TAGLIB_WITH_MP4 OFF CACHE BOOL "" FORCE)
    set(TAGLIB_WITH_MATROSKA OFF CACHE BOOL "" FORCE)
    set(TAGLIB_WITH_OPUS OFF CACHE BOOL "" FORCE)
    set(TAGLIB_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(taglib)
    set(taglib_FOUND TRUE)
endif()

if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "Не найден nlohmann/json. Установите его (vcpkg install nlohmann-json) "
                        "или включите -DAUDIOPLAYER_FETCH_DEPS=ON")
endif()
if(NOT taglib_FOUND)
    message(FATAL_ERROR "Не найден TagLib. Установите его (vcpkg install taglib) "
                        "или включите -DAUDIOPLAYER_FETCH_DEPS=ON")
endif()

# У разных дистрибутивов/способов сборки имя таргета TagLib отличается:
#   TagLib::TagLib / TagLib::tag  — системный пакет, vcpkg
#   taglib / taglib::taglib        — сборка из исходников (FetchContent)
if(TARGET TagLib::TagLib)
    set(AUDIOPLAYER_TAGLIB_TARGET TagLib::TagLib)
elseif(TARGET TagLib::tag)
    set(AUDIOPLAYER_TAGLIB_TARGET TagLib::tag)
elseif(TARGET taglib::taglib)
    set(AUDIOPLAYER_TAGLIB_TARGET taglib::taglib)
elseif(TARGET taglib)
    set(AUDIOPLAYER_TAGLIB_TARGET taglib)
else()
    message(FATAL_ERROR "TagLib найден, но неизвестное имя таргета")
endif()

# --- GUI (этап 3) ---
# Контракты-заголовки (include/...) перечислены в источниках gui:
# AUTOMOC сканирует только заголовки из sources таргета, так moc
# сгенерируется ровно один раз и не будет дублей символов при линковке.
add_library(gui STATIC
    src/gui/ControlPanel.cpp
    src/gui/MainWindow.cpp
    src/gui/SimulatedVisualSource.cpp
    src/gui/Visualizer.cpp
    # Заголовки лежат в include/gui/, а не рядом с .cpp — перечисляем явно,
    # иначе AUTOMOC их не найдёт и не будет vtable (Q_OBJECT).
    include/gui/ControlPanel.hpp
    include/gui/MainWindow.hpp
    include/gui/Visualizer.hpp
    include/gui/IVisualSource.hpp
    include/gui/SimulatedVisualSource.hpp
    include/audio/AudioEngine.hpp
    include/library/Track.hpp
    include/library/MusicLibrary.hpp
    include/library/Playlist.hpp
    include/stats/ListeningStats.hpp
)
target_include_directories(gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gui PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia
)

# --- Приложение ---
# src/stubs/*.cpp — временные реализации контрактов, пока команда
# не принесла настоящие AudioEngine/MusicLibrary/Playlist/ListeningStats.
add_executable(app
    src/main.cpp
    src/stubs/AudioEngineStub.cpp
    src/stubs/MusicLibraryStub.cpp
    src/stubs/PlaylistStub.cpp
    src/stubs/ListeningStatsStub.cpp
)
target_link_libraries(app PRIVATE
    gui
    nlohmann_json::nlohmann_json
    ${AUDIOPLAYER_TAGLIB_TARGET}
)

# Путь к assets на случай запуска не из корня проекта
target_compile_definitions(app PRIVATE
    AUDIOPLAYER_ASSETS_DIR="${AUDIOPLAYER_ASSETS_DIR}"
    AUDIOPLAYER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    # GUI-приложение без консольного окна (уберите WIN32, если нужен вывод в консоль)
    set_target_properties(app PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
