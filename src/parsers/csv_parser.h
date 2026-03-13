#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "../models/data_row.h"
#include "../models/table_schema.h"

namespace parsers {

    // ================================================================== //
    //  Исключение                                                        //
    // ================================================================== //

    class csv_parse_exception : public std::runtime_error {
    public:
        explicit csv_parse_exception(const std::string& message)
            : std::runtime_error(message) {}
    };



    // ================================================================== //
    //  Результат парсинга                                                //
    // ================================================================== //

    struct csv_parse_result {
        models::table_schema schema;
        std::vector<models::data_row> rows;
    };



    // ================================================================== //
    //  csv_parser                                                        //
    // ================================================================== //

    class csv_parser {
    public:
        csv_parser() = delete;

        // Парсит CSV-файл и возвращает схему + строки данных.
        // Первая строка файла — заголовок, разделитель — запятая.
        // Бросает csv_parse_exception при ошибках формата.
        [[nodiscard]] static csv_parse_result parse(const std::filesystem::path& path);

    private:
        // Разбивает строку CSV на поля с учётом кавычек
        [[nodiscard]] static std::vector<std::string> parse_line(std::string_view line);

        // Строит table_schema по заголовкам и всем строкам данных
        [[nodiscard]] static models::table_schema build_schema(const std::string& table_name,
                                                               const std::vector<std::string>& headers,
                                                               const std::vector<std::vector<std::string>>& raw_rows);
    };

} 