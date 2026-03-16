#include <gtest/gtest.h>
#include "utils/type_converter.h"

using utils::type_converter;
using utils::sql_type;
using utils::sql_value;

// ====================================================================== //
//  1. infer_type                                                         //
// ====================================================================== //

TEST(TypeConverter, InferTypeNull) {
    EXPECT_EQ(type_converter::infer_type(""),      sql_type::Null);
    EXPECT_EQ(type_converter::infer_type("null"),  sql_type::Null);
    EXPECT_EQ(type_converter::infer_type("NULL"),  sql_type::Null);
    EXPECT_EQ(type_converter::infer_type("n/a"),   sql_type::Null);
    EXPECT_EQ(type_converter::infer_type("N/A"),   sql_type::Null);
    EXPECT_EQ(type_converter::infer_type("none"),  sql_type::Null);
}

TEST(TypeConverter, InferTypeInteger) {
    EXPECT_EQ(type_converter::infer_type("0"),    sql_type::Integer);
    EXPECT_EQ(type_converter::infer_type("42"),   sql_type::Integer);
    EXPECT_EQ(type_converter::infer_type("-42"),  sql_type::Integer);
    EXPECT_EQ(type_converter::infer_type("+42"),  sql_type::Integer);
    EXPECT_EQ(type_converter::infer_type(" 42 "), sql_type::Integer); // с пробелами
}

TEST(TypeConverter, InferTypeReal) {
    EXPECT_EQ(type_converter::infer_type("3.14"),   sql_type::Real);
    EXPECT_EQ(type_converter::infer_type("-0.5"),   sql_type::Real);
    EXPECT_EQ(type_converter::infer_type("1e10"),   sql_type::Real);
    EXPECT_EQ(type_converter::infer_type("2.5E-3"), sql_type::Real);
    EXPECT_EQ(type_converter::infer_type(" 3.14 "), sql_type::Real);
}

TEST(TypeConverter, InferTypeBoolean) {
    EXPECT_EQ(type_converter::infer_type("true"),  sql_type::Boolean);
    EXPECT_EQ(type_converter::infer_type("false"), sql_type::Boolean);
    EXPECT_EQ(type_converter::infer_type("yes"),   sql_type::Boolean);
    EXPECT_EQ(type_converter::infer_type("no"),    sql_type::Boolean);
    EXPECT_EQ(type_converter::infer_type("on"),    sql_type::Boolean);
    EXPECT_EQ(type_converter::infer_type("off"),   sql_type::Boolean);
    EXPECT_EQ(type_converter::infer_type("True"),  sql_type::Boolean);
}

TEST(TypeConverter, InferTypeText) {
    EXPECT_EQ(type_converter::infer_type("hello"),    sql_type::Text);
    EXPECT_EQ(type_converter::infer_type("!@#"),      sql_type::Text);
    EXPECT_EQ(type_converter::infer_type("1.2.3"),    sql_type::Text);  
    EXPECT_EQ(type_converter::infer_type("abc123"),   sql_type::Text); 
}

TEST(TypeConverter, InferTypeIntegerBeforeBoolean) {
    // "1" и "0" должны быть Integer, а не Boolean
    EXPECT_EQ(type_converter::infer_type("1"), sql_type::Integer);
    EXPECT_EQ(type_converter::infer_type("0"), sql_type::Integer);
}

TEST(TypeConverter, InferTypeSuffixInteger) {
    // Числа с буквенным суффиксом (Eurostat-формат)
    EXPECT_EQ(type_converter::infer_type("42u"),   sql_type::Integer);
    EXPECT_EQ(type_converter::infer_type("42e"),   sql_type::Integer);
}

TEST(TypeConverter, InferTypeSuffixReal) {
    EXPECT_EQ(type_converter::infer_type("3.14u"), sql_type::Real);
    EXPECT_EQ(type_converter::infer_type("2.5e"),  sql_type::Real);
}



// ====================================================================== //
//  2. promote                                                            //
// ====================================================================== //

TEST(TypeConverter, PromoteNullWithAny) {
    EXPECT_EQ(type_converter::promote(sql_type::Null, sql_type::Integer), sql_type::Integer);
    EXPECT_EQ(type_converter::promote(sql_type::Null, sql_type::Real),    sql_type::Real);
    EXPECT_EQ(type_converter::promote(sql_type::Null, sql_type::Boolean), sql_type::Boolean);
    EXPECT_EQ(type_converter::promote(sql_type::Null, sql_type::Text),    sql_type::Text);
    EXPECT_EQ(type_converter::promote(sql_type::Null, sql_type::Null),    sql_type::Null);
}

TEST(TypeConverter, PromoteAnyWithNull) {
    EXPECT_EQ(type_converter::promote(sql_type::Integer, sql_type::Null), sql_type::Integer);
    EXPECT_EQ(type_converter::promote(sql_type::Real,    sql_type::Null), sql_type::Real);
    EXPECT_EQ(type_converter::promote(sql_type::Text,    sql_type::Null), sql_type::Text);
}

TEST(TypeConverter, PromoteSameType) {
    EXPECT_EQ(type_converter::promote(sql_type::Integer, sql_type::Integer), sql_type::Integer);
    EXPECT_EQ(type_converter::promote(sql_type::Real,    sql_type::Real),    sql_type::Real);
    EXPECT_EQ(type_converter::promote(sql_type::Boolean, sql_type::Boolean), sql_type::Boolean);
    EXPECT_EQ(type_converter::promote(sql_type::Text,    sql_type::Text),    sql_type::Text);
}

TEST(TypeConverter, PromoteIntegerAndReal) {
    EXPECT_EQ(type_converter::promote(sql_type::Integer, sql_type::Real), sql_type::Real);
    EXPECT_EQ(type_converter::promote(sql_type::Real, sql_type::Integer), sql_type::Real);
}

TEST(TypeConverter, PromoteBooleanAndInteger) {
    EXPECT_EQ(type_converter::promote(sql_type::Boolean, sql_type::Integer), sql_type::Text);
    EXPECT_EQ(type_converter::promote(sql_type::Integer, sql_type::Boolean), sql_type::Text);
}

TEST(TypeConverter, PromoteBooleanAndReal) {
    EXPECT_EQ(type_converter::promote(sql_type::Boolean, sql_type::Real), sql_type::Text);
    EXPECT_EQ(type_converter::promote(sql_type::Real, sql_type::Boolean), sql_type::Text);
}

TEST(TypeConverter, PromoteAnyWithText) {
    EXPECT_EQ(type_converter::promote(sql_type::Integer, sql_type::Text), sql_type::Text);
    EXPECT_EQ(type_converter::promote(sql_type::Real,    sql_type::Text), sql_type::Text);
    EXPECT_EQ(type_converter::promote(sql_type::Boolean, sql_type::Text), sql_type::Text);
    EXPECT_EQ(type_converter::promote(sql_type::Text,    sql_type::Text), sql_type::Text);
}



// ====================================================================== //
//  3. infer_column_type                                                  //
// ====================================================================== //

TEST(TypeConverter, InferColumnTypeEmpty) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{}), sql_type::Text);
}

TEST(TypeConverter, InferColumnTypeAllNull) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"", "null", "n/a"}), sql_type::Text);
}

TEST(TypeConverter, InferColumnTypeAllInteger) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"1", "2", "3"}), sql_type::Integer);
}

TEST(TypeConverter, InferColumnTypeAllReal) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"1.1", "2.2", "3.3"}), sql_type::Real);
}

TEST(TypeConverter, InferColumnTypeIntegerAndReal) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"1", "2.5", "3"}), sql_type::Real);
}

TEST(TypeConverter, InferColumnTypeIntegerAndNull) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"1", "null", "3"}), sql_type::Integer);
}

TEST(TypeConverter, InferColumnTypeAllBoolean) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"true", "false", "yes"}), sql_type::Boolean);
}

TEST(TypeConverter, InferColumnTypeBooleanAndInteger) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"true", "1", "false"}), sql_type::Text);
}

TEST(TypeConverter, InferColumnTypeMixed) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"1", "hello", "3"}), sql_type::Text);
}

TEST(TypeConverter, InferColumnTypeWithSuffix) {
    EXPECT_EQ(type_converter::infer_column_type(std::vector<std::string>{"3.14u", "2.5e", "1.0"}), sql_type::Real);
}



// ====================================================================== //
//  4. convert                                                            //
// ====================================================================== //

TEST(TypeConverter, ConvertNullLike) {
    EXPECT_EQ(std::get<std::nullptr_t>(type_converter::convert("",     sql_type::Integer)), nullptr);
    EXPECT_EQ(std::get<std::nullptr_t>(type_converter::convert("null", sql_type::Text)),    nullptr);
    EXPECT_EQ(std::get<std::nullptr_t>(type_converter::convert("n/a",  sql_type::Real)),    nullptr);
}

TEST(TypeConverter, ConvertInteger) {
    EXPECT_EQ(std::get<int64_t>(type_converter::convert("42",  sql_type::Integer)), 42);
    EXPECT_EQ(std::get<int64_t>(type_converter::convert("-42", sql_type::Integer)), -42);
    EXPECT_EQ(std::get<int64_t>(type_converter::convert(" 42 ", sql_type::Integer)), 42);
}

TEST(TypeConverter, ConvertIntegerWithSuffix) {
    EXPECT_EQ(std::get<int64_t>(type_converter::convert("42u", sql_type::Integer)), 42);
}

TEST(TypeConverter, ConvertReal) {
    EXPECT_DOUBLE_EQ(std::get<double>(type_converter::convert("3.14", sql_type::Real)), 3.14);
    EXPECT_DOUBLE_EQ(std::get<double>(type_converter::convert("-0.5", sql_type::Real)), -0.5);
}

TEST(TypeConverter, ConvertRealWithSuffix) {
    EXPECT_DOUBLE_EQ(std::get<double>(type_converter::convert("3.14u", sql_type::Real)), 3.14);
}

TEST(TypeConverter, ConvertBoolean) {
    EXPECT_EQ(std::get<bool>(type_converter::convert("true",  sql_type::Boolean)), true);
    EXPECT_EQ(std::get<bool>(type_converter::convert("false", sql_type::Boolean)), false);
    EXPECT_EQ(std::get<bool>(type_converter::convert("yes",   sql_type::Boolean)), true);
    EXPECT_EQ(std::get<bool>(type_converter::convert("no",    sql_type::Boolean)), false);
}

TEST(TypeConverter, ConvertText) {
    EXPECT_EQ(std::get<std::string>(type_converter::convert("hello", sql_type::Text)), "hello");
    EXPECT_EQ(std::get<std::string>(type_converter::convert(" hi ",  sql_type::Text)), "hi");
}

TEST(TypeConverter, ConvertAutoDetect) {
    EXPECT_EQ(std::get<int64_t>(type_converter::convert("42")),     42);
    EXPECT_DOUBLE_EQ(std::get<double>(type_converter::convert("3.14")), 3.14);
    EXPECT_EQ(std::get<bool>(type_converter::convert("true")),      true);
    EXPECT_EQ(std::get<std::string>(type_converter::convert("hello")), "hello");
    EXPECT_EQ(std::get<std::nullptr_t>(type_converter::convert("")), nullptr);
}



// ====================================================================== //
//  5. to_sql_literal                                                     //
// ====================================================================== //

TEST(TypeConverter, ToSqlLiteralNull) {
    EXPECT_EQ(type_converter::to_sql_literal(nullptr), "NULL");
}

TEST(TypeConverter, ToSqlLiteralInteger) {
    EXPECT_EQ(type_converter::to_sql_literal(int64_t{42}),  "42");
    EXPECT_EQ(type_converter::to_sql_literal(int64_t{-42}), "-42");
}

TEST(TypeConverter, ToSqlLiteralReal) {
    EXPECT_EQ(type_converter::to_sql_literal(3.14), "3.14");
}

TEST(TypeConverter, ToSqlLiteralBoolean) {
    EXPECT_EQ(type_converter::to_sql_literal(true),  "1");
    EXPECT_EQ(type_converter::to_sql_literal(false), "0");
}

TEST(TypeConverter, ToSqlLiteralText) {
    EXPECT_EQ(type_converter::to_sql_literal(std::string("hello")),      "'hello'");
    EXPECT_EQ(type_converter::to_sql_literal(std::string("O'Brien")),    "'O''Brien'");
    EXPECT_EQ(type_converter::to_sql_literal(std::string("it's fine")),  "'it''s fine'");
}



// ====================================================================== //
//  6. sql_type_name                                                      //
// ====================================================================== //

TEST(TypeConverter, SqlTypeName) {
    EXPECT_EQ(type_converter::sql_type_name(sql_type::Null),    "TEXT");
    EXPECT_EQ(type_converter::sql_type_name(sql_type::Integer), "INTEGER");
    EXPECT_EQ(type_converter::sql_type_name(sql_type::Real),    "REAL");
    EXPECT_EQ(type_converter::sql_type_name(sql_type::Boolean), "INTEGER");
    EXPECT_EQ(type_converter::sql_type_name(sql_type::Text),    "TEXT");
}



// ====================================================================== //
//  7. type_of                                                            //
// ====================================================================== //

TEST(TypeConverter, TypeOf) {
    EXPECT_EQ(type_converter::type_of(sql_value{nullptr}),         sql_type::Null);
    EXPECT_EQ(type_converter::type_of(sql_value{int64_t{42}}),     sql_type::Integer);
    EXPECT_EQ(type_converter::type_of(sql_value{3.14}),            sql_type::Real);
    EXPECT_EQ(type_converter::type_of(sql_value{true}),            sql_type::Boolean);
    EXPECT_EQ(type_converter::type_of(sql_value{std::string{"x"}}),sql_type::Text);
}