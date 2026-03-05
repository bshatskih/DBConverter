#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace utils {
    
    class string_utils {
    public:
        string_utils() = delete;

        // ====================================================================== //
        //  1. Trim / Strip                                                       //
        // ====================================================================== //

        // Обрезает пробельные символы с обеих сторон
        [[nodiscard]] static std::string trim(std::string_view s);

        // Схлопывает внутренние последовательности пробелов в один пробел.
        [[nodiscard]] static std::string collapse_whitespace(std::string_view s);



        
        // ====================================================================== //
        //  2. Split / Join                                                       //
        // ====================================================================== //

        // Разбивает строку по символу-разделителю.
        [[nodiscard]] static std::vector<std::string> split(std::string_view s, char delimiter);

        // Разбивает строку по строке-разделителю
        [[nodiscard]] static std::vector<std::string> split(std::string_view s, std::string_view delimiter);

        // Соединяет вектор строк через разделитель
        [[nodiscard]] static std::string join(const std::vector<std::string>& parts, std::string_view delimiter);



        // ====================================================================== //
        //  3. Case                                                               //
        // ====================================================================== //

        // Приводит ASCII-символы к нижнему регистру
        [[nodiscard]] static std::string to_lower(std::string_view s);

        // Приводит ASCII-символы к верхнему регистру
        [[nodiscard]] static std::string to_upper(std::string_view s);



        // ====================================================================== //
        //  4. Проверки содержимого                                               //
        // ====================================================================== //

        /// true если строка пуста или состоит только из пробельных символов
        [[nodiscard]] static bool is_blank(std::string_view s);

        // true если строка представляет SQL/CSV null-значение.
        [[nodiscard]] static bool is_null_like(std::string_view s);

        // true если строка содержит корректное целое число.
        [[nodiscard]] static bool is_integer(std::string_view s);

        // true если строка содержит вещественное число.
        // Разрешены: "3.14", "-0.5", "+1e10", "2.5E-3", ".5", "1." 
        // Целые числа тоже возвращают true
        [[nodiscard]] static bool is_real(std::string_view s);

        // true если строка представляет булево значение.
        // Распознаёт (регистронезависимо):
        //   true-like:  "true", "yes", "1", "on"
        //   false-like: "false", "no", "0", "off" 
        [[nodiscard]] static bool is_boolean(std::string_view s);



        // ====================================================================== //
        //  5. Парсинг значений                                                   //
        // ====================================================================== //

        // Пытается распарсить булево значение.
        // true/false или std::nullopt, если строка не является булевой.
        [[nodiscard]] static std::optional<bool> parse_boolean(std::string_view s);

        // Удаляет обрамляющие кавычки и раскрывает escape-последовательности внутри JSON-строки
        // Распакованная строка или std::nullopt при ошибке парсинга.
        [[nodiscard]] static std::optional<std::string> unquote_json_string(std::string_view s);

        // Удаляет обрамляющие кавычки CSV-поля и раскрывает удвоенные кавычки.
        // Если поле не обрамлено кавычками - возвращает строку как есть.
        [[nodiscard]] static std::string unquote_csv_field(std::string_view s);



        // ====================================================================== //
        //  6. Экранирование                                                      //
        // ====================================================================== //

        // Экранирует строку для безопасной вставки в SQL-запрос.
        [[nodiscard]] static std::string escape_sql_string(std::string_view s);

        // Оборачивает идентификатор SQLite в двойные кавычки.
        // Внутренние двойные кавычки удваиваются.
        [[nodiscard]] static std::string quote_sql_identifier(std::string_view s);


        
        // ====================================================================== //
        //  7. Нормализация идентификаторов                                       //
        // ====================================================================== //

        // Превращает произвольную строку в допустимое имя колонки SQLite.
        [[nodiscard]] static std::string to_sql_identifier(std::string_view s);

        // Генерирует уникальное имя, добавляя числовой суффикс при коллизии.
        // Используется для сохранения уникальности заголовков в базе данных.
        [[nodiscard]] static std::string make_unique_identifier(const std::string& base, const std::vector<std::string>& existing);



        // ====================================================================== //
        //  8. Предикаты / поиск                                                  //
        // ====================================================================== //

        /// true если s начинается с prefix (с учётом регистра)
        [[nodiscard]] static bool starts_with(std::string_view s, std::string_view prefix);

        /// true если s заканчивается на suffix (с учётом регистра)
        [[nodiscard]] static bool ends_with(std::string_view s, std::string_view suffix);

        /// true если s содержит подстроку needle (с учётом регистра)
        [[nodiscard]] static bool contains(std::string_view s, std::string_view needle);

        /// Регистронезависимое сравнение двух строк
        [[nodiscard]] static bool iequal(std::string_view a, std::string_view b);

        /// Заменяет все вхождения from на to.
        [[nodiscard]] static std::string replace_all(std::string_view s, std::string_view from, std::string_view to);
    };

} 