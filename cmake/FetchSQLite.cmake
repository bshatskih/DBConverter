# Файл для автоматической загрузки и сборки SQLite
include(FetchContent)

# Подавляем предупреждения
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

message(STATUS "Fetching SQLite3...")

# Скачиваем SQLite3 amalgamation (один файл)
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/sqlite3-src
)

# Скачиваем
FetchContent_MakeAvailable(sqlite3)

# Компилируем SQLite3 в статическую библиотеку
add_library(SQLite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)

# Включаем полезные расширения SQLite
target_compile_definitions(SQLite3 PRIVATE 
    SQLITE_ENABLE_COLUMN_METADATA
    SQLITE_ENABLE_DBSTAT_VTAB
    SQLITE_ENABLE_FTS3
    SQLITE_ENABLE_FTS4
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_UNLOCK_NOTIFY
    SQLITE_ENABLE_MATH_FUNCTIONS
)

# Добавляем пути к заголовкам
target_include_directories(SQLite3 PUBLIC ${sqlite3_SOURCE_DIR})

# Для Windows/MinGW добавляем флаги
if(MINGW)
    target_compile_options(SQLite3 PRIVATE -DSQLITE_OS_WIN=1)
endif()

# Устанавливаем переменную для основного проекта
set(SQLITE_INCLUDE_DIR ${sqlite3_SOURCE_DIR})

message(STATUS "=== SQLite Setup Complete ===")
message(STATUS "SQLite source: ${SQLITE_INCLUDE_DIR}")
message(STATUS "SQLite library: SQLite3")