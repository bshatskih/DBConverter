#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include "../models/table_schema.h"
#include "../models/data_row.h"
#include <nlohmann/json.hpp>







namespace parsers {

    // ================================================================== //
    //  Исключение                                                        //
    // ================================================================== //

    class json_parse_exception : public std::runtime_error {
    public:
        explicit json_parse_exception(const std::string& message)
            : std::runtime_error(message) {}
    };



    // ================================================================== //
    //  Результат парсинга                                                //
    // ================================================================== //

    struct table_with_rows {
        models::table_schema schema;
        std::vector<models::data_row> rows;
        std::vector<int64_t> parent_indices;
    };

    struct json_parse_result {
        std::vector<table_with_rows> tables; 
    };



    // ================================================================== //
    //  Вспомогательная структура                                         //
    // ================================================================== //

    // Один элемент дочернего массива с индексом родительской строки
    struct child_entry {
        int64_t        parent_index;
        nlohmann::json element;
    };



    // ================================================================== //
    //  Парсер                                                            //
    // ================================================================== //

    class json_parser {
     public:
        json_parser() = delete;

        [[nodiscard]] static json_parse_result parse(const std::filesystem::path& path);

     private:

        // Обрабатывает массив объектов и строит таблицу + дочерние таблицы.
        // Все элементы уже собраны вместе с их parent_index.
        static void process_array(
            const std::string& table_name,
            const std::string& parent_table,
            const std::vector<child_entry>& entries,
            std::vector<table_with_rows>& result);

        // Обрабатывает один объект — собирает скалярные поля,
        // разворачивает вложенные объекты, накапливает дочерние массивы.
        static models::data_row process_object(
            const std::string& table_name,
            const nlohmann::json& object,
            std::vector<std::string>& headers,
            int64_t row_index,
            std::unordered_map<std::string, std::vector<child_entry>>& child_arrays);

        // Рекурсивно разворачивает вложенный объект в плоские поля с префиксом.
        // Например: "address": { "city": "Moscow" } → колонка "address_city"
        static void flatten_object(
            const std::string& prefix,
            const nlohmann::json& object,
            models::data_row& row,
            std::vector<std::string>& headers);

        // Сериализует массив примитивов в строку через запятую.
        // Например: ["a", "b", "c"] → "a,b,c"
        [[nodiscard]] static std::string serialize_primitive_array(const nlohmann::json& array);

        // Добавляет заголовок в список если его ещё нет.
        static void ensure_header(
            std::vector<std::string>& headers,
            const std::string& col_name);

        // Определяет типы колонок по всем строкам и строит вектор Column.
        [[nodiscard]] static std::vector<models::Column> infer_columns(
            const std::vector<std::string>& headers,
            const std::vector<models::data_row>& rows);
    };

} 