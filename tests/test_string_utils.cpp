#include <gtest/gtest.h>
#include "utils/string_utils.h"

using utils::string_utils;


// ====================================================================== //
//  1. Trim                                                               //
// ====================================================================== //

TEST(StringUtils, TrimRemovesLeadingSpaces) {
    EXPECT_EQ(string_utils::trim("   hello"), "hello");
}

TEST(StringUtils, TrimRemovesTrailingSpaces) {
    EXPECT_EQ(string_utils::trim("hello   "), "hello");
}

TEST(StringUtils, TrimRemovesBothSides) {
    EXPECT_EQ(string_utils::trim("  hello  "), "hello");
}

TEST(StringUtils, TrimRemovesTabsAndNewlines) {
    EXPECT_EQ(string_utils::trim("\t\nhello\r\n"), "hello");
}

TEST(StringUtils, TrimEmptyString) {
    EXPECT_EQ(string_utils::trim(""), "");
}

TEST(StringUtils, TrimOnlySpaces) {
    EXPECT_EQ(string_utils::trim("   "), "");
}

TEST(StringUtils, TrimNoSpaces) {
    EXPECT_EQ(string_utils::trim("hello"), "hello");
}



// ====================================================================== //
//  2. collapse_whitespace                                                //
// ====================================================================== //

TEST(StringUtils, CollapseWhitespaceMultipleSpaces) {
    EXPECT_EQ(string_utils::collapse_whitespace("hello   world"), "hello world");
}

TEST(StringUtils, CollapseWhitespaceLeadingTrailing) {
    EXPECT_EQ(string_utils::collapse_whitespace("  hello  world  "), "hello world");
}

TEST(StringUtils, CollapseWhitespaceTabsAndSpaces) {
    EXPECT_EQ(string_utils::collapse_whitespace("hello\t\tworld"), "hello world");
}

TEST(StringUtils, CollapseWhitespaceEmpty) {
    EXPECT_EQ(string_utils::collapse_whitespace(""), "");
}

TEST(StringUtils, CollapseWhitespaceOnlySpaces) {
    EXPECT_EQ(string_utils::collapse_whitespace("   "), "");
}



// ====================================================================== //
//  3. Split                                                              //
// ====================================================================== //

TEST(StringUtils, SplitByChar) {
    const auto result = string_utils::split("a,b,c", ',');
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(StringUtils, SplitByCharEmptyParts) {
    const auto result = string_utils::split("a,,b", ',');
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[1], "");
}

TEST(StringUtils, SplitByCharNoDelimiter) {
    const auto result = string_utils::split("hello", ',');
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello");
}

TEST(StringUtils, SplitByString) {
    const auto result = string_utils::split("a::b::c", "::");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(StringUtils, SplitByEmptyDelimiter) {
    const auto result = string_utils::split("abc", "");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}



// ====================================================================== //
//  4. Join                                                               //
// ====================================================================== //

TEST(StringUtils, JoinBasic) {
    EXPECT_EQ(string_utils::join({"a", "b", "c"}, ","), "a,b,c");
}

TEST(StringUtils, JoinMulticharDelimiter) {
    EXPECT_EQ(string_utils::join({"a", "b", "c"}, ", "), "a, b, c");
}

TEST(StringUtils, JoinSingleElement) {
    EXPECT_EQ(string_utils::join({"hello"}, ","), "hello");
}

TEST(StringUtils, JoinEmpty) {
    EXPECT_EQ(string_utils::join({}, ","), "");
}



// ====================================================================== //
//  5. Case                                                               //
// ====================================================================== //

TEST(StringUtils, ToLower) {
    EXPECT_EQ(string_utils::to_lower("Hello WORLD"), "hello world");
}

TEST(StringUtils, ToUpper) {
    EXPECT_EQ(string_utils::to_upper("Hello World"), "HELLO WORLD");
}

TEST(StringUtils, ToLowerAlreadyLower) {
    EXPECT_EQ(string_utils::to_lower("hello"), "hello");
}



// ====================================================================== //
//  6. Проверки содержимого                                               //
// ====================================================================== //

TEST(StringUtils, IsBlankEmpty) {
    EXPECT_TRUE(string_utils::is_blank(""));
}

TEST(StringUtils, IsBlankSpaces) {
    EXPECT_TRUE(string_utils::is_blank("   \t\n"));
}

TEST(StringUtils, IsBlankNotBlank) {
    EXPECT_FALSE(string_utils::is_blank("  a  "));
}

TEST(StringUtils, IsNullLikeEmpty) {
    EXPECT_TRUE(string_utils::is_null_like(""));
}

TEST(StringUtils, IsNullLikeNull) {
    EXPECT_TRUE(string_utils::is_null_like("null"));
    EXPECT_TRUE(string_utils::is_null_like("NULL"));
    EXPECT_TRUE(string_utils::is_null_like("Null"));
}

TEST(StringUtils, IsNullLikeNA) {
    EXPECT_TRUE(string_utils::is_null_like("n/a"));
    EXPECT_TRUE(string_utils::is_null_like("N/A"));
    EXPECT_TRUE(string_utils::is_null_like("na"));
}

TEST(StringUtils, IsNullLikeNotNull) {
    EXPECT_FALSE(string_utils::is_null_like("hello"));
    EXPECT_FALSE(string_utils::is_null_like("0"));
}

TEST(StringUtils, IsIntegerPositive) {
    EXPECT_TRUE(string_utils::is_integer("42"));
    EXPECT_TRUE(string_utils::is_integer("+42"));
    EXPECT_TRUE(string_utils::is_integer("-42"));
}

TEST(StringUtils, IsIntegerInvalid) {
    EXPECT_FALSE(string_utils::is_integer(""));
    EXPECT_FALSE(string_utils::is_integer("3.14"));
    EXPECT_FALSE(string_utils::is_integer("abc"));
    EXPECT_FALSE(string_utils::is_integer("+"));
    EXPECT_FALSE(string_utils::is_integer("-"));
}

TEST(StringUtils, IsReal) {
    EXPECT_TRUE(string_utils::is_real("3.14"));
    EXPECT_TRUE(string_utils::is_real("-0.5"));
    EXPECT_TRUE(string_utils::is_real("1e10"));
    EXPECT_TRUE(string_utils::is_real("2.5E-3"));
    EXPECT_TRUE(string_utils::is_real("42"));  
}

TEST(StringUtils, IsRealInvalid) {
    EXPECT_FALSE(string_utils::is_real(""));
    EXPECT_FALSE(string_utils::is_real("abc"));
    EXPECT_FALSE(string_utils::is_real("1.2.3"));
}

TEST(StringUtils, IsBoolean) {
    EXPECT_TRUE(string_utils::is_boolean("true"));
    EXPECT_TRUE(string_utils::is_boolean("True"));
    EXPECT_TRUE(string_utils::is_boolean("TRUE"));
    EXPECT_TRUE(string_utils::is_boolean("false"));
    EXPECT_TRUE(string_utils::is_boolean("yes"));
    EXPECT_TRUE(string_utils::is_boolean("no"));
    EXPECT_TRUE(string_utils::is_boolean("on"));
    EXPECT_TRUE(string_utils::is_boolean("off"));
    EXPECT_TRUE(string_utils::is_boolean("1"));
    EXPECT_TRUE(string_utils::is_boolean("0"));
}

TEST(StringUtils, IsBooleanInvalid) {
    EXPECT_FALSE(string_utils::is_boolean(""));
    EXPECT_FALSE(string_utils::is_boolean("maybe"));
    EXPECT_FALSE(string_utils::is_boolean("2"));
}



// ====================================================================== //
//  7. Парсинг значений                                                   //
// ====================================================================== //

TEST(StringUtils, ParseBooleanTrue) {
    EXPECT_EQ(string_utils::parse_boolean("true"),  std::make_optional(true));
    EXPECT_EQ(string_utils::parse_boolean("yes"),   std::make_optional(true));
    EXPECT_EQ(string_utils::parse_boolean("1"),     std::make_optional(true));
    EXPECT_EQ(string_utils::parse_boolean("on"),    std::make_optional(true));
}

TEST(StringUtils, ParseBooleanFalse) {
    EXPECT_EQ(string_utils::parse_boolean("false"), std::make_optional(false));
    EXPECT_EQ(string_utils::parse_boolean("no"),    std::make_optional(false));
    EXPECT_EQ(string_utils::parse_boolean("0"),     std::make_optional(false));
    EXPECT_EQ(string_utils::parse_boolean("off"),   std::make_optional(false));
}

TEST(StringUtils, ParseBooleanNullopt) {
    EXPECT_EQ(string_utils::parse_boolean("maybe"), std::nullopt);
    EXPECT_EQ(string_utils::parse_boolean(""),      std::nullopt);
}

TEST(StringUtils, UnquoteJsonString) {
    EXPECT_EQ(string_utils::unquote_json_string("\"hello\""),       std::make_optional(std::string("hello")));
    EXPECT_EQ(string_utils::unquote_json_string("\"O\\\"Brien\""),  std::make_optional(std::string("O\"Brien")));
    EXPECT_EQ(string_utils::unquote_json_string("\"hello\\nworld\""), std::make_optional(std::string("hello\nworld")));
}

TEST(StringUtils, UnquoteJsonStringInvalid) {
    EXPECT_EQ(string_utils::unquote_json_string("hello"),   std::nullopt); 
    EXPECT_EQ(string_utils::unquote_json_string("\"hello"), std::nullopt); 
    EXPECT_EQ(string_utils::unquote_json_string("\"\\q\""), std::nullopt); 
}

TEST(StringUtils, UnquoteCsvField) {
    EXPECT_EQ(string_utils::unquote_csv_field("hello"),          "hello");
    EXPECT_EQ(string_utils::unquote_csv_field("\"hello\""),      "hello");
    EXPECT_EQ(string_utils::unquote_csv_field("\"O\"\"Brien\""), "O\"Brien");
}



// ====================================================================== //
//  8. Нормализация идентификаторов                                       //
// ====================================================================== //

TEST(StringUtils, ToSqlIdentifierBasic) {
    EXPECT_EQ(string_utils::to_sql_identifier("hello"), "hello");
}

TEST(StringUtils, ToSqlIdentifierSpaces) {
    EXPECT_EQ(string_utils::to_sql_identifier("hello world"), "hello_world");
}

TEST(StringUtils, ToSqlIdentifierDashes) {
    EXPECT_EQ(string_utils::to_sql_identifier("hello-world"), "hello_world");
}

TEST(StringUtils, ToSqlIdentifierUpperCase) {
    EXPECT_EQ(string_utils::to_sql_identifier("Hello World"), "hello_world");
}

TEST(StringUtils, ToSqlIdentifierStartsWithDigit) {
    EXPECT_EQ(string_utils::to_sql_identifier("1hello"), "col_1hello");
}

TEST(StringUtils, ToSqlIdentifierEmpty) {
    EXPECT_EQ(string_utils::to_sql_identifier(""), "col_unknown");
}

TEST(StringUtils, ToSqlIdentifierOnlySpecialChars) {
    EXPECT_EQ(string_utils::to_sql_identifier("!!!"), "col_unknown");
}

TEST(StringUtils, MakeUniqueIdentifierNoCollision) {
    EXPECT_EQ(string_utils::make_unique_identifier("name", {"id", "age"}), "name");
}

TEST(StringUtils, MakeUniqueIdentifierCollision) {
    EXPECT_EQ(string_utils::make_unique_identifier("name", {"id", "name"}), "name_1");
}

TEST(StringUtils, MakeUniqueIdentifierMultipleCollisions) {
    EXPECT_EQ(string_utils::make_unique_identifier("name", {"name", "name_1", "name_2"}), "name_3");
}



// ====================================================================== //
//  9. Предикаты / поиск                                                  //
// ====================================================================== //

TEST(StringUtils, StartsWith) {
    EXPECT_TRUE(string_utils::starts_with("hello world", "hello"));
    EXPECT_FALSE(string_utils::starts_with("hello world", "world"));
    EXPECT_TRUE(string_utils::starts_with("hello", ""));
}

TEST(StringUtils, EndsWith) {
    EXPECT_TRUE(string_utils::ends_with("hello world", "world"));
    EXPECT_FALSE(string_utils::ends_with("hello world", "hello"));
    EXPECT_TRUE(string_utils::ends_with("hello", ""));
}

TEST(StringUtils, Contains) {
    EXPECT_TRUE(string_utils::contains("hello world", "lo wo"));
    EXPECT_FALSE(string_utils::contains("hello world", "xyz"));
}

TEST(StringUtils, IEqual) {
    EXPECT_TRUE(string_utils::iequal("Hello", "hello"));
    EXPECT_TRUE(string_utils::iequal("HELLO", "hello"));
    EXPECT_FALSE(string_utils::iequal("hello", "world"));
    EXPECT_FALSE(string_utils::iequal("hello", "hello!"));
}

TEST(StringUtils, ReplaceAll) {
    EXPECT_EQ(string_utils::replace_all("hello world world", "world", "earth"), "hello earth earth");
    EXPECT_EQ(string_utils::replace_all("hello", "xyz", "abc"), "hello");
    EXPECT_EQ(string_utils::replace_all("hello", "", "abc"), "hello");
}