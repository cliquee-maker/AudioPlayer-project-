find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_database
    test_database.cpp
)
target_link_libraries(test_database PRIVATE
    persistence
    Qt6::Test
)

add_executable(test_music_library
    test_music_library.cpp
)
target_link_libraries(test_music_library PRIVATE
    persistence
    Qt6::Test
)

add_test(NAME test_database COMMAND test_database)
add_test(NAME test_music_library COMMAND test_music_library)
