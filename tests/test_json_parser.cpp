#include <gtest/gtest.h>
#include "parsers/json_parser.h"
#include "models/table_schema.h"
#include <filesystem>

using parsers::json_parser;
using parsers::json_parse_exception;

// Путь к тестовым данным
static const std::filesystem::path TEST_DATA = TEST_DATA_DIR;

// ====================================================================== //
//  1. Базовый массив объектов                                           //
// ====================================================================== //

TEST(JsonParser, ParseBasicArray) {
    const auto result = json_parser::parse(TEST_DATA / "basic.json");

    ASSERT_EQ(result.tables.size(), 1u);
    EXPECT_EQ(result.tables[0].schema.name(), "basic");
    EXPECT_EQ(result.tables[0].rows.size(), 3u);
}

TEST(JsonParser, ParseBasicArrayHasCustomPk) {
    const auto result = json_parser::parse(TEST_DATA / "basic.json");

    EXPECT_TRUE(result.tables[0].schema.has_custom_pk());
    EXPECT_EQ(result.tables[0].schema.pk_column(), "id");
}

TEST(JsonParser, ParseBasicArrayColumnTypes) {
    const auto result = json_parser::parse(TEST_DATA / "basic.json");
    const auto& schema = result.tables[0].schema;

    const auto name_col = schema.find_column("name");
    ASSERT_TRUE(name_col.has_value());
    EXPECT_EQ(name_col->type, models::Sqlite_type::TEXT);

    const auto age_col = schema.find_column("age");
    ASSERT_TRUE(age_col.has_value());
    EXPECT_EQ(age_col->type, models::Sqlite_type::INTEGER);
}

TEST(JsonParser, ParseBasicArrayRowValues) {
    const auto result = json_parser::parse(TEST_DATA / "basic.json");
    const auto& rows  = result.tables[0].rows;

    EXPECT_EQ(rows[0].get("name").value(), "Alice");
    EXPECT_EQ(rows[0].get("age").value(),  "30");
    EXPECT_EQ(rows[1].get("name").value(), "Bob");
    EXPECT_EQ(rows[2].get("name").value(), "Charlie");
}



// ====================================================================== //
//  2. Одиночный объект                                                  //
// ====================================================================== //

TEST(JsonParser, ParseSingleObject) {
    const auto result = json_parser::parse(TEST_DATA / "single_object.json");

    ASSERT_EQ(result.tables.size(), 1u);
    EXPECT_EQ(result.tables[0].rows.size(), 1u);
    EXPECT_EQ(result.tables[0].rows[0].get("name").value(), "Alice");
}



// ====================================================================== //
//  3. Вложенный объект — flatten                                        //
// ====================================================================== //

TEST(JsonParser, ParseNestedObjectFlattened) {
    const auto result = json_parser::parse(TEST_DATA / "nested_object.json");

    // Одна таблица — вложенный объект разворачивается в плоские колонки
    ASSERT_EQ(result.tables.size(), 1u);
    const auto& schema = result.tables[0].schema;

    EXPECT_TRUE(schema.has_column("address_city"));
    EXPECT_TRUE(schema.has_column("address_zip"));
}

TEST(JsonParser, ParseNestedObjectValues) {
    const auto result = json_parser::parse(TEST_DATA / "nested_object.json");
    const auto& rows  = result.tables[0].rows;

    EXPECT_EQ(rows[0].get("address_city").value(), "Moscow");
    EXPECT_EQ(rows[0].get("address_zip").value(),  "101000");
    EXPECT_EQ(rows[1].get("address_city").value(), "Berlin");
}



// ====================================================================== //
//  4. Вложенный массив объектов — дочерняя таблица                     //
// ====================================================================== //

TEST(JsonParser, ParseNestedArrayTwoTables) {
    const auto result = json_parser::parse(TEST_DATA / "nested_array.json");

    ASSERT_EQ(result.tables.size(), 2u);
    EXPECT_EQ(result.tables[0].schema.name(), "nested_array");
    EXPECT_EQ(result.tables[1].schema.name(), "nested_array_orders");
}

TEST(JsonParser, ParseNestedArrayParentRows) {
    const auto result = json_parser::parse(TEST_DATA / "nested_array.json");

    EXPECT_EQ(result.tables[0].rows.size(), 2u);
}

TEST(JsonParser, ParseNestedArrayChildRows) {
    const auto result = json_parser::parse(TEST_DATA / "nested_array.json");

    // Alice — 2 заказа, Bob — 1 заказ
    EXPECT_EQ(result.tables[1].rows.size(), 3u);
}

TEST(JsonParser, ParseNestedArrayChildIsChild) {
    const auto result = json_parser::parse(TEST_DATA / "nested_array.json");
    const auto& child_schema = result.tables[1].schema;

    EXPECT_TRUE(child_schema.is_child());
    EXPECT_EQ(child_schema.parent_table(), "nested_array");
    EXPECT_EQ(child_schema.foreign_key(),  "nested_array_id");
}

TEST(JsonParser, ParseNestedArrayParentIndices) {
    const auto result = json_parser::parse(TEST_DATA / "nested_array.json");
    const auto& child = result.tables[1];

    // Первые два заказа принадлежат строке 0 (Alice)
    // Третий заказ принадлежит строке 1 (Bob)
    ASSERT_EQ(child.parent_indices.size(), 3u);
    EXPECT_EQ(child.parent_indices[0], 0);
    EXPECT_EQ(child.parent_indices[1], 0);
    EXPECT_EQ(child.parent_indices[2], 1);
}



// ====================================================================== //
//  5. Массив примитивов — сериализация                                  //
// ====================================================================== //

TEST(JsonParser, ParsePrimitiveArraySerialized) {
    const auto result = json_parser::parse(TEST_DATA / "primitive_array.json");

    ASSERT_EQ(result.tables.size(), 1u);
    const auto& rows = result.tables[0].rows;

    EXPECT_EQ(rows[0].get("types").value(), "Grass,Poison");
    EXPECT_EQ(rows[1].get("types").value(), "Fire");
}

TEST(JsonParser, ParsePrimitiveArrayColumnIsText) {
    const auto result = json_parser::parse(TEST_DATA / "primitive_array.json");
    const auto types_col = result.tables[0].schema.find_column("types");

    ASSERT_TRUE(types_col.has_value());
    EXPECT_EQ(types_col->type, models::Sqlite_type::TEXT);
}



// ====================================================================== //
//  6. NULL значения и отсутствующие поля                                //
// ====================================================================== //

TEST(JsonParser, ParseNullValues) {
    const auto result = json_parser::parse(TEST_DATA / "nulls.json");
    const auto& rows  = result.tables[0].rows;

    // Alice — email есть
    EXPECT_FALSE(rows[0].is_null("email"));

    // Bob — email: null
    EXPECT_TRUE(rows[1].is_null("email"));

    // Charlie — поле email отсутствует → тоже NULL
    EXPECT_TRUE(rows[2].is_null("email"));
}



// ====================================================================== //
//  7. Булевые значения                                                  //
// ====================================================================== //

TEST(JsonParser, ParseBooleanValues) {
    const auto result = json_parser::parse(TEST_DATA / "booleans.json");
    const auto& rows  = result.tables[0].rows;

    EXPECT_EQ(rows[0].get("active").value(),   "1");
    EXPECT_EQ(rows[1].get("active").value(),   "0");
    EXPECT_EQ(rows[0].get("verified").value(), "0");
    EXPECT_EQ(rows[1].get("verified").value(), "1");
}

TEST(JsonParser, ParseBooleanColumnType) {
    const auto result   = json_parser::parse(TEST_DATA / "booleans.json");
    const auto& schema  = result.tables[0].schema;
    const auto active   = schema.find_column("active");

    ASSERT_TRUE(active.has_value());
    EXPECT_EQ(active->type, models::Sqlite_type::INTEGER);
}



// ====================================================================== //
//  8. Ошибки                                                            //
// ====================================================================== //

TEST(JsonParser, ParseInvalidJson) {
    EXPECT_THROW(
        (void)json_parser::parse(TEST_DATA / "invalid.json"),
        json_parse_exception);
}

TEST(JsonParser, ParseNonExistentFile) {
    EXPECT_THROW(
        (void)json_parser::parse(TEST_DATA / "nonexistent.json"),
        json_parse_exception);
}

TEST(JsonParser, ParseWrongExtension) {
    EXPECT_THROW(
        (void)json_parser::parse(TEST_DATA / "file.txt"),
        json_parse_exception);
}
