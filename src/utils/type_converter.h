#pragma once
#include "string_utils.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>



namespace utils {

    // Тип SQLite-колонки (для DDL и выбора стратегии привязки параметра).
    enum class sql_type : uint8_t {
        Null,
        Integer,
        Real,
        Boolean,
        Text
    };

    // Типизированное значение, готовое к передаче в sqlite3_bind_*.
    using sql_value = std::variant<std::nullptr_t, int64_t, double, bool, std::string>;







    class type_converter {
     public:

        type_converter() = delete;

        // ====================================================================== //
        //  Детекция типа                                                          //
        // ====================================================================== //

        // Определяет наиболее специфичный sql_type для одного строкового значения.
        // Пробелы вокруг значения игнорируются (trim применяется внутри).
        [[nodiscard]] static sql_type infer_type(std::string_view s);

        // Выводит единый тип для целой колонки по всем её строковым значениям.
        [[nodiscard]] static sql_type infer_column_type(const std::vector<std::string_view>& values);

        // Перегрузка для вектора std::string
        [[nodiscard]] static sql_type infer_column_type(const std::vector<std::string>& values);


  
        // ====================================================================== //
        //  Конвертация значения                                                   //
        // ====================================================================== //

        // Конвертирует строку в sql_value, опираясь на заранее известный тип колонки.
        [[nodiscard]] static sql_value convert(std::string_view s, sql_type col_type);

        // Конвертирует строку без подсказки о типе (автодетекция).
        // Эквивалентно convert(s, infer_type(s)).
        [[nodiscard]] static sql_value convert(std::string_view s);



        // ====================================================================== //
        //  Сериализация                                                           //
        // ====================================================================== //

        // Возвращает SQL-литерал для значения (для отладки и DDL-скриптов)
        [[nodiscard]] static std::string to_sql_literal(const sql_value& value);

        // Возвращает имя SQLite-типа для использования в DDL.
        [[nodiscard]] static std::string_view sql_type_name(sql_type type);



        // ====================================================================== //
        //  Вспомогательные утилиты                                               //
        // ====================================================================== //

        // Повышает тип current до наименее специфичного, вмещающего оба типа.
        [[nodiscard]] static sql_type promote(sql_type current, sql_type incoming);

        // Возвращает sql_type, соответствующий хранимой альтернативе variant.
        [[nodiscard]] static sql_type type_of(const sql_value& value);
    };

} 