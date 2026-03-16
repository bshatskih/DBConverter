#include <gtest/gtest.h>
#include "parsers/csv_parser.h"
#include "models/table_schema.h"
#include <filesystem>

using parsers::csv_parser;
using parsers::csv_parse_exception;

// Путь к тестовым данным
static const std::filesystem::path TEST_DATA = TEST_DATA_DIR;


// ====================================================================== //
//  1. parse_line                                                         //
// ====================================================================== //

TEST(CsvParser, ParseLineBasic) {
    const auto fields = csv_parser::parse_line("a,b,c");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[0], "a");
    EXPECT_EQ(fields[1], "b");
    EXPECT_EQ(fields[2], "c");
}

TEST(CsvParser, ParseLineQuotedFieldWithComma) {
    const auto fields = csv_parser::parse_line("1,\"hello, world\",3");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[1], "hello, world");
}

TEST(CsvParser, ParseLineDoubleQuoteEscape) {
    const auto fields = csv_parser::parse_line("\"O\"\"Brien\"");
    ASSERT_EQ(fields.size(), 1u);
    EXPECT_EQ(fields[0], "O\"Brien");
}

TEST(CsvParser, ParseLineEmptyFields) {
    const auto fields = csv_parser::parse_line("a,,c");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[1], "");
}

TEST(CsvParser, ParseLineAllEmpty) {
    const auto fields = csv_parser::parse_line(",,");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[0], "");
    EXPECT_EQ(fields[1], "");
    EXPECT_EQ(fields[2], "");
}

TEST(CsvParser, ParseLineSingleField) {
    const auto fields = csv_parser::parse_line("hello");
    ASSERT_EQ(fields.size(), 1u);
    EXPECT_EQ(fields[0], "hello");
}

TEST(CsvParser, ParseLineEmptyQuotedField) {
    const auto fields = csv_parser::parse_line("a,\"\",c");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[1], "");
}

TEST(CsvParser, ParseLineUnterminatedQuote) {
    EXPECT_THROW((void)csv_parser::parse_line("\"hello"), csv_parse_exception);
}



// ====================================================================== //
//  2. parse - базовый сценарий                                           //
// ====================================================================== //

TEST(CsvParser, ParseBasicFile) {
    const auto result = csv_parser::parse(TEST_DATA / "basic.csv");

    EXPECT_EQ(result.schema.name(), "basic");
    EXPECT_EQ(result.rows.size(), 3u);
    EXPECT_EQ(result.schema.column_count(), 3u); // id -> PK, не в columns()
}

TEST(CsvParser, ParseBasicFileHasCustomPk) {
    const auto result = csv_parser::parse(TEST_DATA / "basic.csv");
    EXPECT_TRUE(result.schema.has_custom_pk());
    EXPECT_EQ(result.schema.pk_column(), "id");
}

TEST(CsvParser, ParseBasicFileColumnTypes) {
    const auto result = csv_parser::parse(TEST_DATA / "basic.csv");
    const auto& cols  = result.schema.columns();

    // name -> TEXT
    const auto name_col = result.schema.find_column("name");
    ASSERT_TRUE(name_col.has_value());
    EXPECT_EQ(name_col->type, models::Sqlite_type::TEXT);

    // age -> INTEGER
    const auto age_col = result.schema.find_column("age");
    ASSERT_TRUE(age_col.has_value());
    EXPECT_EQ(age_col->type, models::Sqlite_type::INTEGER);

    // score -> REAL
    const auto score_col = result.schema.find_column("score");
    ASSERT_TRUE(score_col.has_value());
    EXPECT_EQ(score_col->type, models::Sqlite_type::REAL);
}

TEST(CsvParser, ParseBasicFileRowValues) {
    const auto result = csv_parser::parse(TEST_DATA / "basic.csv");

    EXPECT_EQ(result.rows[0].get("name").value(), "Alice");
    EXPECT_EQ(result.rows[0].get("age").value(),  "30");
    EXPECT_EQ(result.rows[1].get("name").value(), "Bob");
    EXPECT_EQ(result.rows[2].get("name").value(), "Charlie");
}



// ====================================================================== //
//  3. parse - кавычки                                                    //
// ====================================================================== //

TEST(CsvParser, ParseQuotedFields) {
    const auto result = csv_parser::parse(TEST_DATA / "quoted.csv");

    EXPECT_EQ(result.rows[0].get("description").value(), "Hello, world");
    EXPECT_EQ(result.rows[1].get("description").value(), "He said \"hi\"");
}



// ====================================================================== //
//  4. parse - NULL значения                                              //
// ====================================================================== //

TEST(CsvParser, ParseNullValues) {
    const auto result = csv_parser::parse(TEST_DATA / "nulls.csv");

    // Bob — пустые поля → NULL
    EXPECT_TRUE(result.rows[1].is_null("email"));
    EXPECT_TRUE(result.rows[1].is_null("age"));

    // Charlie — "null" → NULL
    EXPECT_TRUE(result.rows[2].is_null("age"));
}



// ====================================================================== //
//  5. parse - булевые значения                                           //
// ====================================================================== //

TEST(CsvParser, ParseBooleanValues) {
    const auto result = csv_parser::parse(TEST_DATA / "booleans.csv");

    // Булевые нормализуются в "1"/"0"
    EXPECT_EQ(result.rows[0].get("active").value(),   "1");
    EXPECT_EQ(result.rows[1].get("active").value(),   "0");
    EXPECT_EQ(result.rows[0].get("verified").value(), "1");
    EXPECT_EQ(result.rows[1].get("verified").value(), "0");
}

TEST(CsvParser, ParseBooleanColumnType) {
    const auto result = csv_parser::parse(TEST_DATA / "booleans.csv");

    const auto active_col = result.schema.find_column("active");
    ASSERT_TRUE(active_col.has_value());
    EXPECT_EQ(active_col->type, models::Sqlite_type::INTEGER);
}



// ====================================================================== //
//  6. parse - дублирующиеся заголовки                                   //
// ====================================================================== //

TEST(CsvParser, ParseDuplicateHeaders) {
    const auto result = csv_parser::parse(TEST_DATA / "duplicate_headers.csv");

    // Второй "name" должен стать "name_1"
    EXPECT_TRUE(result.schema.has_column("name"));
    EXPECT_TRUE(result.schema.has_column("name_1"));
}



// ====================================================================== //
//  7. parse - ошибки                                                     //
// ====================================================================== //

TEST(CsvParser, ParseMismatchedFieldCount) {
    EXPECT_THROW(
        (void)csv_parser::parse(TEST_DATA / "mismatched.csv"),
        csv_parse_exception);
}

TEST(CsvParser, ParseNonExistentFile) {
    EXPECT_THROW(
        (void)csv_parser::parse(TEST_DATA / "nonexistent.csv"),
        csv_parse_exception);
}

TEST(CsvParser, ParseWrongExtension) {
    EXPECT_THROW(
        (void)csv_parser::parse(TEST_DATA / "file.txt"),
        csv_parse_exception);
}