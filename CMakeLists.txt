# Указывает, что для сборки проекта нужен CMake версии не ниже 3.14. 
# Если версия меньше — CMake выдаст фатальную ошибку и остановится.
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(DBConverter VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Директории для выходных файлов
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Включаем нашу конфигурацию для SQLite
include(cmake/FetchSQLite.cmake)

# Включаем нашу конфигурацию для nlohmann/json
include(cmake/FetchJSON.cmake)

# Подключаем Google Test
include(cmake/FetchGTest.cmake)

# Включаем поддержку тестов в CMake
enable_testing()


# Список исходников
set(LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/utils/file_validator.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/string_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/type_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/models/table_schema.cpp
    ${CMAKE_SOURCE_DIR}/src/models/data_row.cpp
    ${CMAKE_SOURCE_DIR}/src/database/db_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/parsers/csv_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/parsers/json_parser.cpp
)


# Добавляем исполняемый файл
add_executable(${PROJECT_NAME} src/main.cpp ${LIB_SOURCES})

# Линковка с SQLite
target_link_libraries(${PROJECT_NAME}
    PRIVATE SQLite3
)

# Явно указываем пути к заголовочным файлам
target_include_directories(${PROJECT_NAME}
    PRIVATE ${SQLITE_INCLUDE_DIR}
    PRIVATE ${CMAKE_BINARY_DIR}/lib
    PRIVATE ${CMAKE_SOURCE_DIR}/src
)


# ------------------------------------------------------------------ #
#  Тесты                                                             #
# ------------------------------------------------------------------ #

add_executable(DBConverter_tests
    tests/test_string_utils.cpp
    tests/test_type_converter.cpp
    tests/test_csv_parser.cpp
    tests/test_json_parser.cpp
    tests/test_db_manager.cpp
    tests/test_integration.cpp
    ${LIB_SOURCES}
)

target_link_libraries(DBConverter_tests 
    PRIVATE GTest::gtest_main
    PRIVATE SQLite3
)

target_include_directories(DBConverter_tests
    PRIVATE ${SQLITE_INCLUDE_DIR}
    PRIVATE ${CMAKE_BINARY_DIR}/lib
    PRIVATE ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(DBConverter_tests
    PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/test_data"
)

include(GoogleTest)
gtest_discover_tests(DBConverter_tests)



message(STATUS "Project configured successfully!")
message(STATUS "SQLite include: ${SQLITE_INCLUDE_DIR}")
message(STATUS "nlohmann/json path: ${CMAKE_SOURCE_DIR}/libs/nlohmann/json.hpp")


