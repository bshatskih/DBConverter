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

# Добавляем исполняемый файл
add_executable(${PROJECT_NAME} src/main.cpp src/utils/file_validator.cpp
                                            src/utils/string_utils.cpp)

# Линковка с SQLite
target_link_libraries(${PROJECT_NAME}
    PRIVATE SQLite3
)

# Явно указываем пути к заголовочным файлам
target_include_directories(${PROJECT_NAME}
    PRIVATE ${SQLITE_INCLUDE_DIR}
)

message(STATUS "Project configured successfully!")
message(STATUS "SQLite include: ${SQLITE_INCLUDE_DIR}")