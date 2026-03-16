#include "json_parser.h"
#include "../utils/file_validator.h"
#include "../utils/string_utils.h"
#include "../utils/type_converter.h"
#include <fstream>
#include <algorithm>
#include <ranges>



namespace parsers {

    // ================================================================== //
    //  Основной метод                                                    //
    // ================================================================== //

    json_parse_result json_parser::parse(const std::filesystem::path& path) {
        utils::validation_result validation = utils::file_validator().validate(path);
        if (!validation) {
            throw json_parse_exception("File validation failed: " + validation.error);
        }

        std::ifstream fin(path);
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(fin);
        } catch (const nlohmann::json::parse_error& e) {
            throw json_parse_exception(
                "JSON parse error in '" + path.string() + "': " + e.what());
        }

        if (!root.is_array() && !root.is_object()) {
            throw json_parse_exception(
                "JSON root must be an array or object: '" + path.string() + "'");
        }

        if (root.is_object()) {
            root = nlohmann::json::array({root});
        }

        std::string table_name = utils::string_utils::to_sql_identifier(path.stem().string());

        // Оборачиваем корневой массив в child_entry с parent_index = -1
        std::vector<child_entry> entries;
        entries.reserve(root.size());
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, root.size())) {
            entries.push_back({-1, root[i]});
        }

        json_parse_result result;
        process_array(table_name, "", entries, result.tables);
        return result;
    }



    // ================================================================== //
    //  Обработка массива объектов                                        //
    // ================================================================== //

    void json_parser::process_array(
        const std::string& table_name,
        const std::string& parent_table,
        const std::vector<child_entry>& entries,
        std::vector<table_with_rows>& result)
    {
        std::vector<models::data_row> raw_rows;
        std::vector<std::string> headers;
        std::vector<int64_t> parent_indices;

        // child_arrays: field_name -> все элементы со всех родителей
        std::unordered_map<std::string, std::vector<child_entry>> child_arrays;

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, entries.size())) {
            if (!entries[i].element.is_object()) {
                throw json_parse_exception(
                    "Expected array of objects for table '" + table_name + "'");
            }

            models::data_row row = process_object(
                table_name, entries[i].element, headers, static_cast<int64_t>(i), child_arrays);

            raw_rows.push_back(std::move(row));
            parent_indices.push_back(entries[i].parent_index);
        }

        // Определяем типы колонок
        std::vector<models::Column> columns = infer_columns(headers, raw_rows);

        // Ищем колонку id
        std::string pk_column = "id";
        bool has_custom_pk = false;
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, headers.size())) {
            if (headers[i] == "id") {
                has_custom_pk = true;
                break;
            }
        }
        if (has_custom_pk) {
            columns.erase(
                std::remove_if(columns.begin(), columns.end(),
                    [](const models::Column& col) { return col.name == "id"; }),
                columns.end());
        }

        // Нормализуем булевые значения в "1"/"0"
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, raw_rows.size())) {
            for (std::size_t j : std::ranges::iota_view(std::size_t{0}, columns.size())) {
                const auto val = raw_rows[i].get(columns[j].name);
                if (val.has_value() && utils::string_utils::is_boolean(*val)) {
                    const auto maybe = utils::type_converter::convert(*val, utils::sql_type::Boolean);
                    const bool b = std::get<bool>(maybe);
                    raw_rows[i].set(columns[j].name, b ? "1" : "0");
                }
            }
        }

        // Строим схему
        bool is_child  = !parent_table.empty();
        std::string fk_column = is_child ? parent_table + "_id" : "";

        models::table_schema schema = is_child
            ? models::table_schema(table_name, columns, pk_column, has_custom_pk, parent_table, fk_column)
            : models::table_schema(table_name, columns, pk_column, has_custom_pk);

        // Родительская таблица идёт первой
        result.push_back({
            std::move(schema),
            std::move(raw_rows),
            is_child ? std::move(parent_indices) : std::vector<int64_t>{}
        });

        // Рекурсивно обрабатываем дочерние массивы -
        // каждый field_name порождает одну дочернюю таблицу
        for (auto& [field_name, child_entries] : child_arrays) {
            std::string child_table_name = table_name + "_" + field_name;
            process_array(child_table_name, table_name, child_entries, result);
        }
    }



    // ================================================================== //
    //  Обработка одного объекта                                          //
    // ================================================================== //

    models::data_row json_parser::process_object(
        const std::string& table_name,
        const nlohmann::json& object,
        std::vector<std::string>& headers,
        int64_t row_index,
        std::unordered_map<std::string, std::vector<child_entry>>& child_arrays)
    {
        models::data_row row;

        // nlohmann::json::items() не поддерживает iota_view напрямую -
        // используем индексный доступ через keys()
        const auto keys = object.items();
        std::vector<std::string> key_list;
        for (const auto& [k, v] : keys) {
            key_list.push_back(k);
        }

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, key_list.size())) {
            const std::string& key = key_list[i];
            const auto& value = object.at(key);
            const std::string col_name = utils::string_utils::to_sql_identifier(key);

            if (value.is_null()) {
                ensure_header(headers, col_name);
                row.set_null(col_name);

            } else if (value.is_boolean()) {
                ensure_header(headers, col_name);
                row.set(col_name, value.get<bool>() ? "1" : "0");

            } else if (value.is_number_integer()) {
                ensure_header(headers, col_name);
                row.set(col_name, std::to_string(value.get<int64_t>()));

            } else if (value.is_number_float()) {
                ensure_header(headers, col_name);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.15g", value.get<double>());
                row.set(col_name, std::string(buf));

            } else if (value.is_string()) {
                ensure_header(headers, col_name);
                row.set(col_name, value.get<std::string>());

            } else if (value.is_object()) {
                flatten_object(col_name, value, row, headers);

            } else if (value.is_array()) {
                if (value.empty()) {
                    continue;
                }

                if (value[0].is_object()) {
                    for (std::size_t j : std::ranges::iota_view(std::size_t{0}, value.size())) {
                        child_arrays[col_name].push_back({row_index, value[j]});
                    }
                } else {
                    ensure_header(headers, col_name);
                    row.set(col_name, serialize_primitive_array(value));
                }
            }
        }

        return row;
    }



    // ================================================================== //
    //  Разворачивание вложенного объекта в плоские поля                 //
    // ================================================================== //

    void json_parser::flatten_object(
        const std::string& prefix,
        const nlohmann::json& object,
        models::data_row& row,
        std::vector<std::string>& headers)
    {
        std::vector<std::string> key_list;
        for (const auto& [k, v] : object.items()) {
            key_list.push_back(k);
        }

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, key_list.size())) {
            const std::string& key = key_list[i];
            const auto& value = object.at(key);
            const std::string col_name = prefix + "_" + utils::string_utils::to_sql_identifier(key);

            if (value.is_null()) {
                ensure_header(headers, col_name);
                row.set_null(col_name);

            } else if (value.is_boolean()) {
                ensure_header(headers, col_name);
                row.set(col_name, value.get<bool>() ? "1" : "0");

            } else if (value.is_number_integer()) {
                ensure_header(headers, col_name);
                row.set(col_name, std::to_string(value.get<int64_t>()));

            } else if (value.is_number_float()) {
                ensure_header(headers, col_name);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.15g", value.get<double>());
                row.set(col_name, std::string(buf));

            } else if (value.is_string()) {
                ensure_header(headers, col_name);
                row.set(col_name, value.get<std::string>());

            } else if (value.is_object()) {
                flatten_object(col_name, value, row, headers);

            } else if (value.is_array()) {
                if (!value.empty() && !value[0].is_object()) {
                    ensure_header(headers, col_name);
                    row.set(col_name, serialize_primitive_array(value));
                }
                // Массив объектов внутри flatten - пропускаем
            }
        }
    }



    // ================================================================== //
    //  Определение типов колонок                                        //
    // ================================================================== //

    std::vector<models::Column> json_parser::infer_columns(
        const std::vector<std::string>& headers,
        const std::vector<models::data_row>& rows)
    {
        std::vector<models::Column> columns;
        columns.reserve(headers.size());

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, headers.size())) {
            std::vector<std::string> col_values;
            col_values.reserve(rows.size());

            for (std::size_t j : std::ranges::iota_view(std::size_t{0}, rows.size())) {
                const auto val = rows[j].get(headers[i]);
                col_values.push_back(val.has_value() ? *val : "");
            }

            utils::sql_type sql_t = utils::type_converter::infer_column_type(col_values);

            models::Sqlite_type sqlite_t;
            switch (sql_t) {
                case utils::sql_type::Integer:
                case utils::sql_type::Boolean:
                    sqlite_t = models::Sqlite_type::INTEGER; break;
                case utils::sql_type::Real:
                    sqlite_t = models::Sqlite_type::REAL;    break;
                case utils::sql_type::Null:
                case utils::sql_type::Text:
                    sqlite_t = models::Sqlite_type::TEXT;    break;
            }
            columns.push_back({headers[i], sqlite_t});
        }

        return columns;
    }



    // ================================================================== //
    //  Сериализация массива примитивов                                   //
    // ================================================================== //

    std::string json_parser::serialize_primitive_array(const nlohmann::json& array) {
        std::string result;

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, array.size())) {
            const auto& el = array[i];

            if (el.is_string()) {
                result += el.get<std::string>();
            } else if (el.is_boolean()) {
                result += el.get<bool>() ? "1" : "0";
            } else if (el.is_number_integer()) {
                result += std::to_string(el.get<int64_t>());
            } else if (el.is_number_float()) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.15g", el.get<double>());
                result += buf;
            } else if (el.is_null()) {
                result += "null";
            }

            if (i + 1 < array.size()) {
                result += ",";
            }
        }

        return result;
    }



    // ================================================================== //
    //  Вспомогательные методы                                            //
    // ================================================================== //

    void json_parser::ensure_header(
        std::vector<std::string>& headers,
        const std::string& col_name)
    {
        if (std::find(headers.begin(), headers.end(), col_name) == headers.end()) {
            headers.push_back(col_name);
        }
    }

} 