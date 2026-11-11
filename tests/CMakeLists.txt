# Тесты линкуются с тем же таргетом library, что и приложение.
# Qt Test уже входит в Qt6::Test — отдельная зависимость (Catch2) не нужна.
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_playlist test_playlist.cpp)
target_link_libraries(tst_playlist PRIVATE library Qt6::Test)
add_test(NAME playlist COMMAND tst_playlist)

add_executable(tst_musiclibrary test_musiclibrary.cpp)
target_link_libraries(tst_musiclibrary PRIVATE library Qt6::Test)
add_test(NAME musiclibrary COMMAND tst_musiclibrary)
