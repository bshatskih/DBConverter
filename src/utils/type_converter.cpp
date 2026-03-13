#include "type_converter.h"
#include <cassert>
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <ranges>





namespace utils {

    
    // ========================================================================== //
    //  Вспомогательные утилиты                                                  //
    // ========================================================================== //

    sql_type type_converter::type_of(const sql_value& value) {
        return std::visit([](const auto& v) -> sql_type {
            using T = std::decay_t<decltype(v)>;
            
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return sql_type::Null;
            }
            if constexpr (std::is_same_v<T, int64_t>) {
                return sql_type::Integer;
            }
            if constexpr (std::is_same_v<T, double>) {
                return sql_type::Real;
            }
            if constexpr (std::is_same_v<T, bool>) {
                return sql_type::Boolean;
            }
            if constexpr (std::is_same_v<T, std::string>) {
                return sql_type::Text;
            }
        }, value);
    }


    sql_type type_converter::promote(sql_type current, sql_type incoming) {
        // Null поглощается любым конкретным типом
        if (current == sql_type::Null) {
            return incoming;
        }
        if (incoming == sql_type::Null) {
            return current;
        }

        // Одинаковые типы - без изменений
        if (current == incoming) {
            return current;
        }
        
        // Integer + Real -> Real (расширение без потерь)
        if ((current == sql_type::Integer && incoming == sql_type::Real) || (current == sql_type::Real && incoming == sql_type::Integer)) {
            return sql_type::Real;
        }

        // Boolean + Integer / Boolean + Real -> Text
        if (current == sql_type::Boolean || incoming == sql_type::Boolean) {
            return sql_type::Text;
        }

        // Любой конфликт -> Text
        return sql_type::Text;
    }



    // ========================================================================== //
    //  Детекция типа                                                             //
    // ========================================================================== //

    sql_type type_converter::infer_type(std::string_view str) {
        std::string trimmed_str = string_utils::trim(str);

        if (string_utils::is_null_like(trimmed_str)) {
            return sql_type::Null;
        }

        if (string_utils::is_integer(trimmed_str)) {
            return sql_type::Integer;
        }

        if (string_utils::is_real(trimmed_str)) {
            return sql_type::Real;
        }
        if (string_utils::is_boolean(trimmed_str)) {
            return sql_type::Boolean;
        }


        std::size_t end = trimmed_str.size();
        while (end > 0 && std::isalpha(static_cast<unsigned char>(trimmed_str[end - 1]))) {
            --end;
        }
        if (end < trimmed_str.size() && end > 0) {
            std::string_view stripped(trimmed_str.data(), end);
            if (string_utils::is_integer(stripped)) {
                return sql_type::Integer;
            }
            if (string_utils::is_real(stripped)) {
                return sql_type::Real;
            }
        }

        return sql_type::Text;
    }


    sql_type type_converter::infer_column_type(const std::vector<std::string_view>& values) {
        // На случай пустой колонки возвращаем Text (безопасный дефолт для DDL)
        if (values.empty()) {
            return sql_type::Text;
        }

        sql_type result = sql_type::Null;
        bool has_non_null = false;

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, values.size())) {
            sql_type t = infer_type(values[i]);
            if (t == sql_type::Null) {
               continue;
            }

            has_non_null = true;
            result = promote(result, t);

            // Text - финальное состояние, дальнейший обход бессмысленен
            if (result == sql_type::Text) {
                return sql_type::Text;
            }
        }

        // Если все значения null-like — возвращаем Text (безопасный дефолт)
        return has_non_null ? result : sql_type::Text;
    }


    sql_type type_converter::infer_column_type(const std::vector<std::string>& values) {
        std::vector<std::string_view> views;
        views.reserve(values.size());
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, values.size())) {
             views.emplace_back(values[i]);
        }

        return infer_column_type(views);
    }



    // ========================================================================== //
    //  Конвертация значения                                                      //
    // ========================================================================== //

    sql_value type_converter::convert(std::string_view str, sql_type col_type) {
        // Null-like всегда -> nullptr_t
        if (string_utils::is_null_like(str)) {
            return nullptr;
        }

        std::string trimmed = string_utils::trim(str);

        // Вспомогательная лямбда — возвращает строку без буквенного суффикса
        const auto strip_suffix = [&]() -> std::string_view {
            std::size_t end = trimmed.size();
            while (end > 0 && std::isalpha(static_cast<unsigned char>(trimmed[end - 1]))) {
                --end;
            }
            return std::string_view(trimmed.data(), end);
        };

        switch (col_type) {
            case sql_type::Null:
                // Колонка объявлена как Null, но значение не null - fallback to Text
                return std::string(trimmed);

            case sql_type::Integer: {
                int64_t result = 0;
                std::from_chars_result res = std::from_chars(
                    trimmed.data(), trimmed.data() + trimmed.size(), result);

                if (res.ec == std::errc{} && res.ptr == trimmed.data() + trimmed.size()) {
                    return result;
                }

                const std::string_view stripped = strip_suffix();
                if (stripped.size() < trimmed.size() && !stripped.empty()) {
                    res = std::from_chars(stripped.data(), stripped.data() + stripped.size(), result);
                    if (res.ec == std::errc{} && res.ptr == stripped.data() + stripped.size()) {
                        return result;
                    }
                }

                return nullptr;
            }

            case sql_type::Real: {
                double result = 0.0;
                std::from_chars_result res = std::from_chars(
                    trimmed.data(), trimmed.data() + trimmed.size(), result);

                if (res.ec == std::errc{} && res.ptr == trimmed.data() + trimmed.size()) {
                    return result;
                }

                const std::string_view stripped = strip_suffix();
                if (stripped.size() < trimmed.size() && !stripped.empty()) {
                    res = std::from_chars(stripped.data(), stripped.data() + stripped.size(), result);
                    if (res.ec == std::errc{} && res.ptr == stripped.data() + stripped.size()) {
                        return result;
                    }
                }

                return nullptr;
            }

            case sql_type::Boolean: {
                std::optional<bool> maybe = string_utils::parse_boolean(str);

                if (maybe.has_value()) {
                    return *maybe;
                }

                // Не удалось - fallback to nullptr;
                return nullptr;
            }

            case sql_type::Text:
                return std::move(trimmed);
        }

        return std::move(trimmed);
    }


    sql_value type_converter::convert(std::string_view str) {
        return convert(str, infer_type(str));
    }



    // ========================================================================== //
    //  Сериализация                                                              //
    // ========================================================================== //

    std::string type_converter::to_sql_literal(const sql_value& value) {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return "NULL";

            }
            if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(v);

            } 
            if constexpr (std::is_same_v<T, double>) {
                char buf[32];
                std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general, 15);
                return std::string(buf, res.ptr);
            } 
            if constexpr (std::is_same_v<T, bool>) {
                return v ? "1" : "0";

            } 
            if constexpr (std::is_same_v<T, std::string>) {
                // Оборачиваем в одинарные кавычки, внутренние - удваиваем
                return '\'' + string_utils::escape_sql_string(v) + '\'';
            }
        }, value);
    }


    std::string_view type_converter::sql_type_name(sql_type type) {
        switch (type) {
            case sql_type::Null:    return "TEXT";
            case sql_type::Integer: return "INTEGER";
            case sql_type::Real:    return "REAL";
            case sql_type::Boolean: return "INTEGER"; 
            case sql_type::Text:    return "TEXT";
        }
        return "TEXT"; 
    }


} 