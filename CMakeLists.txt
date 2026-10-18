cmake_minimum_required(VERSION 3.22)
project(AudioPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Системные зависимости (pacman) ---
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)
find_package(nlohmann_json REQUIRED)
find_package(taglib REQUIRED)

# --- Dear ImGui (imgui-sfml v3.0 требует >=1.91.1 и указания IMGUI_DIR) ---
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)   # у ImGui нет CMakeLists — просто скачивает исходники

# --- ImGui-SFML ---
FetchContent_Declare(imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG        v3.0
    GIT_SHALLOW    TRUE)
set(BUILD_SHARED_LIBS ON)   # иначе imgui-sfml требует статический SFML, которого нет в pacman
set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)
FetchContent_MakeAvailable(imgui-sfml)
set(BUILD_SHARED_LIBS OFF)  # вернуть для остальных целей (gui — явно STATIC)

# --- Приложение (smoke-тест этапа 1) ---
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE
    ImGui-SFML::ImGui-SFML
    SFML::Graphics SFML::Window SFML::System SFML::Audio
    nlohmann_json::nlohmann_json
    TagLib::tag)

# Этап 3: add_library(gui STATIC src/gui/*.cpp) — добавим, когда появятся файлы gui/