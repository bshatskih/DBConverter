#include "string_utils.h"
#include <algorithm>
#include <cassert>
#include <ranges>
#include <cstdint>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <charconv>
#include <unordered_set>


namespace utils {

    // ========================================================================== //
    //  Внутренние вспомогательные методы                                         //
    // ========================================================================== //

    namespace {

        constexpr bool is_whitespace(char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v';
        }

        // Набор null-like значений (в нижнем регистре)
        const std::unordered_set<std::string_view> null_like_values = {
            "", "null", "nil", "none", "n/a", "na", "nan"
        };

        // true/false лексемы
        const std::unordered_set<std::string_view> true_values  = {"true",  "yes", "1", "on"};
        const std::unordered_set<std::string_view> false_values = {"false", "no",  "0", "off"};

    } 



    // ========================================================================== //
    //  1. Trim / Strip                                                           //
    // ========================================================================== //
    
    std::string string_utils::trim(std::string_view str) {
        auto left  = std::ranges::find_if_not(str, is_whitespace);
        auto right = std::ranges::find_if_not(str | std::views::reverse, is_whitespace);
        if (left == str.end()) {
            return {};
        }
        return std::string(left, right.base());
    }


    std::string string_utils::collapse_whitespace(std::string_view str) {
        std::string result;
        result.reserve(str.size());

        bool is_contain_space = false;
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, str.size())) {
            if (is_whitespace(str[i])) {
                if (!is_contain_space && !result.empty()) {
                    result += ' ';
                    is_contain_space = true;
                }
            } else {
                result += str[i];
                is_contain_space = false;
            }
        }

        if (!result.empty() && result.back() == ' ') {
            result.pop_back();
        }

        return result;
    }



    // ========================================================================== //
    //  2. Split / Join                                                           //
    // ========================================================================== //

    std::vector<std::string> string_utils::split(std::string_view str, char delimiter) {
        std::vector<std::string> result;
        std::size_t start = 0;

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, str.size())) {
            if (str[i] == delimiter) {
                result.emplace_back(str.substr(start, i - start));
                start = i + 1;
            }
        }
        result.emplace_back(str.substr(start, str.size() - start));
        return result;
    }


    std::vector<std::string> string_utils::split(std::string_view str, std::string_view delimiter) {
        std::vector<std::string> result;
        
        if (delimiter.empty()) {
            // Разбиваем по символам - хотя вообще говоря можно было бы и ошибку выбросить
            result.reserve(str.size());
            
            for (std::size_t i : std::ranges::iota_view(std::size_t{0}, str.size())) { 
                result.emplace_back(1, str[i]); 
            }
            return result;
        }

        std::size_t start = 0;
        while (true) {
            std::size_t pos = str.find(delimiter, start);
            
            if (pos == std::string_view::npos) {
                result.emplace_back(str.substr(start, str.size() - start));
                break;
            }
            result.emplace_back(str.substr(start, pos - start));
            start = pos + delimiter.size();
        }
        return result;
    }


    std::string string_utils::join(const std::vector<std::string>& parts, std::string_view delimiter) {
        if (parts.empty()) {
            return {};
        }

        std::string result;
        std::size_t total = delimiter.size() * (parts.size() - 1);
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, parts.size())) { 
            total += parts[i].size();
        }
        result.reserve(total);

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, parts.size() - 1)) { 
            result += parts[i];
            result += delimiter;
        }
        result += parts.back();
        return result;
    }



    // ========================================================================== //
    //  3. Case                                                                   //
    // ========================================================================== //

    std::string string_utils::to_lower(std::string_view s) {
        std::string result(s);
        std::ranges::transform(result, result.begin(), [](unsigned char c) { return std::tolower(c); });
        return result;
    }


    std::string string_utils::to_upper(std::string_view s) {
        std::string result(s);
        std::ranges::transform(result, result.begin(), [](unsigned char c) { return std::toupper(c); });
        return result;
    }



    // ========================================================================== //
    //  4. Проверки содержимого                                                   //
    // ========================================================================== //

    bool string_utils::is_blank(std::string_view str) {
        return std::ranges::all_of(str, is_whitespace);
    }


    bool string_utils::is_null_like(std::string_view str) {
        const std::string lower = to_lower(str);
        return null_like_values.contains(lower);
    }


    bool string_utils::is_integer(std::string_view str) {
        if (str.empty()) {
            return false;
        }

        std::size_t start = 0;
        if (str[0] == '+' || str[0] == '-') {
            start = 1;
        }

        if (start == str.size()) {
            return false; 
        }

        return std::ranges::all_of(str.substr(start, str.size() - start), [](unsigned char c) { return std::isdigit(c) != 0; });
    }


    bool string_utils::is_real(std::string_view s) {
        if (s.empty()) {
            return false;
        }

        double value;
        std::from_chars_result result = std::from_chars(s.data(), s.data() + s.size(), value);

        bool parsed_without_error = (result.ec == std::errc{});
        bool entire_string_consumed = (result.ptr == s.data() + s.size());

        return parsed_without_error && entire_string_consumed;
    }


    bool string_utils::is_boolean(std::string_view str) {
        const std::string lower = to_lower(str);
        return true_values.contains(lower) || false_values.contains(lower);
    }



    // ========================================================================== //
    //  5. Парсинг значений                                                       //
    // ========================================================================== //

    std::optional<bool> string_utils::parse_boolean(std::string_view s) {
        const std::string lower = to_lower(s);
        
        if (true_values.contains(lower)) {
            return std::make_optional(true);
        } 
        if (false_values.contains(lower)) {
            return std::make_optional(false);
        }

        return std::nullopt;
    }


    std::optional<std::string> string_utils::unquote_json_string(std::string_view s) {

        // Удаляет обрамляющие кавычки и раскрывает escape-последовательности внутри JSON-строки 
        // (если строка не обрамлена кавычками, возвращаем std::nullopt - или это баг парсера, или файл повреждён).
        if (s.size() < 2 || s.front() != '"' || s.back() != '"') {
            return std::nullopt;
        }
        s = s.substr(1, s.size() - 2);

        std::string result;
        result.reserve(s.size());

        for (std::size_t i = 0; i < s.size(); ++i) {
            // Если текущий символ не является обратной косой чертой, просто добавляем его к результату,
            // в противном случае обрабатываем escape-последовательность.
            if (s[i] != '\\') {
                result += s[i];
                continue;
            }

            // Обрыв escape-последовательности - некорректная строка
            if (i + 1 >= s.size()) {
                return std::nullopt; 
            }
             
            ++i;
            switch (s[i]) {
                case '"':  
                    result += '"';  
                    break;
                case '\\': 
                    result += '\\'; 
                    break;
                case '/':  
                    result += '/';  
                    break;
                case 'b':  
                    result += '\b'; 
                    break;
                case 'f':  
                    result += '\f'; 
                    break;
                case 'n':  
                    result += '\n'; 
                    break;
                case 'r':  
                    result += '\r'; 
                    break;
                case 't':  
                    result += '\t'; 
                    break;
                case 'u': {
                    // \uXXXX - поддержка BMP-символов в формате UTF-16
                    if (i + 4 >= s.size()) {
                        return std::nullopt;
                    }

                    std::string hex(s.substr(i + 1, 4));
                    uint32_t codepoint = 0;
                    std::from_chars_result tmp_res = std::from_chars(hex.data(), hex.data() + 4, codepoint, 16);

                    if (tmp_res.ec != std::errc{} || tmp_res.ptr != hex.data() + 4) {
                        return std::nullopt;
                    }
                    
                    // Кодируем codepoint в UTF-8
                    i += 4;
                    if (codepoint <= 0x7F) {
                        result += static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        result += static_cast<char>(0xC0 | (codepoint >> 6));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 |  (codepoint >> 12));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 |  (codepoint & 0x3F));
                    }
                    break;
                }
                // неизвестная escape-последовательность - некорректная строка
                default:
                    return std::nullopt;  
            }
        }
        return result;
    }
    

    std::string string_utils::unquote_csv_field(std::string_view s) {
        // есть если строка с двух сторон не заключена в кавычки, то мы просто её возвращаем в том же виде
        if (s.size() < 2 || s.front() != '"' || s.back() != '"') {
            return std::string(s);
        }
        s = s.substr(1, s.size() - 2);

        std::string result;
        result.reserve(s.size());

        for (std::size_t i = 0; i < s.size(); ++i) {
            result += s[i];
            // Удвоенная кавычка внутри конвертируется в одну кавычку
            if (s[i] == '"' && i + 1 < s.size() && s[i + 1] == '"') {
                ++i;
            }
        }
        return result;
    }



    // ========================================================================== //
    //  6. Экранирование                                                          //
    // ========================================================================== //

    std::string string_utils::escape_sql_string(std::string_view s) {
        std::string result;
        result.reserve(s.size() + 4);

        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, s.size())) {
            // удваиваем одинарную кавычку
            if (s[i] == '\'') {
                result += '\'';  
            }
            result += s[i];
        }
        return result;
    }


    std::string string_utils::quote_sql_identifier(std::string_view s) {
        std::string result;
        result.reserve(s.size() + 4);
        result += '"';
        
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, s.size())) {
            // удваиваем кавычку
            if (s[i] == '"') {
                result += '"';  
            }
            result += s[i];
        }
        result += '"';
        return result;
    }



    // ========================================================================== //
    //  7. Нормализация идентификаторов                                           //
    // ========================================================================== //

    std::string string_utils::to_sql_identifier(std::string_view s) {
        // Шаги 1–2: 
        std::string work = collapse_whitespace(trim(s));

        // Шаг 3–4:
        std::string result;
        result.reserve(work.size() + 4);
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, work.size())) {
            if (std::isalnum(work[i]) || work[i] == '_') {
                result += work[i];
            } else if (work[i] == ' ' || work[i] == '-') {
                result += '_';
            }
        }

        // Шаг 5:
        if (!result.empty() && std::isdigit(result[0])) {
            result = "col_" + result;
        }

        // Шаг 6: 
        if (result.empty()) {
            return "col_unknown";
        }

        // Шаг 7: 
        return to_lower(result);
    }


    std::string string_utils::make_unique_identifier(const std::string& base, const std::vector<std::string>& existing) {
        // Быстрая проверка без коллизии
        const auto exists = [&](const std::string& name) {
            return std::find(existing.begin(), existing.end(), name) != existing.end();
        };

        if (!exists(base)) {
            return base;
        }

        for (std::size_t counter = 1; ; ++counter) {
            std::string candidate = base + "_" + std::to_string(counter);
            if (!exists(candidate)) {
                return candidate;
            }
        }
    }



    // ========================================================================== //
    //  8. Предикаты / поиск                                                     //
    // ========================================================================== //

    bool string_utils::starts_with(std::string_view s, std::string_view prefix) {
        return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
    }


    bool string_utils::ends_with(std::string_view s, std::string_view suffix) {
        return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
    }


    bool string_utils::contains(std::string_view s, std::string_view needle) {
        return s.find(needle) != std::string_view::npos;
    }


    bool string_utils::iequal(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) {
            return false;
        }
        
        return std::equal(a.begin(), a.end(), b.begin(),
                        [](unsigned char x, unsigned char y) {
                            return std::tolower(x) == std::tolower(y);
                        });
    }


    std::string string_utils::replace_all(std::string_view s, std::string_view from, std::string_view to) {
        if (from.empty()) {
            return std::string(s);
        }

        std::string result;
        result.reserve(s.size());

        std::size_t start = 0;
        while (true) {
            std::size_t pos = s.find(from, start);
            if (pos == std::string_view::npos) {
                result += s.substr(start);
                break;
            }
            result += s.substr(start, pos - start);
            result += to;
            start = pos + from.size();
        }
        return result;
    }

}