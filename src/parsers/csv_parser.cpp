#include "csv_parser.h"
#include "../utils/string_utils.h"
#include "../utils/type_converter.h"
#include "../utils/file_validator.h"
#include <ranges>
#include <algorithm>
#include <fstream>

namespace parsers {

    // ================================================================== //
    //  Основной метод                                                    //
    // ================================================================== //

    csv_parse_result csv_parser::parse(const std::filesystem::path& path) {
        // Валидация файла перед парсингом
        const utils::validation_result validation = utils::file_validator().validate(path);
        if (!validation) {
            throw csv_parse_exception("File validation failed: " + validation.error);
        }

        std::ifstream fin(path);

        // Читаем заголовок
        std::string header_line;
        if (!std::getline(fin, header_line)) {
            throw csv_parse_exception("Failed to read header row: '" + path.string() + "'");
        }

        // Убираем BOM если есть (UTF-8 BOM: EF BB BF)
        // BOM (Byte Order Mark) - это специальная метка в начале файла, которая указывает на кодировку и порядок байт.
        if (header_line.size() >= 3 &&
            static_cast<unsigned char>(header_line[0]) == 0xEF &&
            static_cast<unsigned char>(header_line[1]) == 0xBB &&
            static_cast<unsigned char>(header_line[2]) == 0xBF) {
            header_line = header_line.substr(3);
        }

        // Убираем \r если файл в Windows-формате
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }

        // Разбиваем строку на подстроки по запятым, учитывая кавычки
        const std::vector<std::string> raw_headers = parse_line(header_line);
        if (raw_headers.empty()) {
            throw csv_parse_exception("Header row is empty: '" + path.string() + "'");
        }

        // Нормализуем заголовки в SQL-идентификаторы
        std::vector<std::string> headers;
        headers.reserve(raw_headers.size());
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, raw_headers.size())) {
            std::string identifier = utils::string_utils::to_sql_identifier(raw_headers[i]);
            identifier = utils::string_utils::make_unique_identifier(identifier, headers);
            headers.push_back(std::move(identifier));
        }

        // Читаем все строки данных
        std::vector<std::vector<std::string>> raw_rows;
        std::string line;
        std::size_t line_number = 1;

        while (std::getline(fin, line)) {
            ++line_number;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Пропускаем пустые строки
            if (utils::string_utils::is_blank(line)) {
                continue;
            }

            // Разбиваем строку на подстроки по запятым, учитывая кавычки
            std::vector<std::string> fields = parse_line(line);

            // Проверяем что количество полей совпадает с заголовком
            if (fields.size() != headers.size()) {
                throw csv_parse_exception(
                    "Row " + std::to_string(line_number) +
                    " has " + std::to_string(fields.size()) +
                    " fields, expected " + std::to_string(headers.size()));
            }

            raw_rows.push_back(std::move(fields));
        }

        // Строим схему таблицы
        const std::string table_name = utils::string_utils::to_sql_identifier(path.stem().string());

        models::table_schema schema = build_schema(table_name, headers, raw_rows);

        // Строим data_row для каждой строки
        std::vector<models::data_row> rows;
        rows.reserve(raw_rows.size());

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, raw_rows.size())) {
            const std::vector<std::string>& raw_row = raw_rows[i];
            models::data_row row;
            for (std::size_t j : std::ranges::iota_view(std::size_t{0}, headers.size())) {
                const std::string val = utils::string_utils::trim(raw_row[j]);

                if (utils::string_utils::is_null_like(val)) {
                    row.set_null(headers[j]);
                } else if (utils::string_utils::is_boolean(val)) {
                    utils::sql_value maybe = utils::type_converter::convert(val, utils::sql_type::Boolean);
                    bool b = std::get<bool>(maybe);
                    row.set(headers[j], b ? "1" : "0");
                } else {
                    row.set(headers[j], val);
                }
            }
            rows.push_back(std::move(row));
        }

        return csv_parse_result{std::move(schema), std::move(rows)};
    }



    // ================================================================== //
    //  Парсинг строки                                                    //
    // ================================================================== //

    std::vector<std::string> csv_parser::parse_line(std::string_view line) {
        std::vector<std::string> fields;
        std::string current;
        bool in_quotes = false;

        for (std::size_t i = 0; i < line.size(); ++i) {
            if (in_quotes) {
                if (line[i] == '"') {
                    // Удвоенная кавычка внутри поля -> одна кавычка
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        current += '"';
                        ++i;
                    } else {
                        // Закрывающая кавычка
                        in_quotes = false;
                    }
                } else {
                    current += line[i];
                }
            } else {
                if (line[i] == '"') {
                    in_quotes = true;
                } else if (line[i] == ',') {
                    fields.push_back(std::move(current));
                    current.clear();
                } else {
                    current += line[i];
                }
            }
        }

        if (in_quotes) {
            throw csv_parse_exception("Unterminated quoted field: " + std::string(line));
        }

        fields.push_back(std::move(current));
        return fields;
    }



    // ================================================================== //
    //  Построение схемы                                                  //
    // ================================================================== //

    models::table_schema csv_parser::build_schema(const std::string& table_name,
                                                  const std::vector<std::string>& headers,
                                                  const std::vector<std::vector<std::string>>& raw_rows) {
        std::vector<models::Column> columns;
        columns.reserve(headers.size());

        for (std::size_t col_idx : std::ranges::iota_view(std::size_t{0}, headers.size())) {
            // Собираем все значения колонки
            std::vector<std::string> col_values;
            col_values.reserve(raw_rows.size());
            for (std::size_t row_idx : std::ranges::iota_view(std::size_t{0}, raw_rows.size())) {
                col_values.push_back(raw_rows[row_idx][col_idx]);
            }

            // Определяем тип колонки
            const utils::sql_type sql_t = utils::type_converter::infer_column_type(col_values);

            models::Sqlite_type sqlite_t;
            switch (sql_t) {
                case utils::sql_type::Integer:
                case utils::sql_type::Boolean:
                    sqlite_t = models::Sqlite_type::INTEGER;
                    break;
                case utils::sql_type::Real:
                    sqlite_t = models::Sqlite_type::REAL;
                    break;
                case utils::sql_type::Null:
                case utils::sql_type::Text:
                    sqlite_t = models::Sqlite_type::TEXT;
                    break;
            }

            columns.push_back({headers[col_idx], sqlite_t});
        }


        std::string pk_column = "id";
        bool has_custom_pk = false;

        for (std::size_t col_idx : std::ranges::iota_view(std::size_t{0}, headers.size())) {
            if (headers[col_idx] == "id") {
                pk_column = "id";
                has_custom_pk = true;
                break;
            }
        }

        // Если нашли PK - убираем его из колонок, он хранится отдельно в table_schema
        if (has_custom_pk) {
            columns.erase(
                std::remove_if(columns.begin(), columns.end(),
                    [&pk_column](const models::Column& col) {
                        return col.name == pk_column;
                    }),
                columns.end());
        }

        return models::table_schema(table_name, std::move(columns), pk_column, has_custom_pk);
    }

} 