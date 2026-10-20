cmake_minimum_required(VERSION 3.22)
project(AudioPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Qt (Qt Widgets) ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- Системные зависимости (pacman) ---
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)
find_package(nlohmann_json REQUIRED)
find_package(taglib REQUIRED)

# --- Приложение (smoke-тест этапа 1) ---
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia
    nlohmann_json::nlohmann_json
    TagLib::tag
)

# Этап 3: add_library(gui STATIC src/gui/*.cpp) — добавим, когда появятся файлы gui/
