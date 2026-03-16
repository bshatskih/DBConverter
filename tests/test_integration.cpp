#include <gtest/gtest.h>
#include "parsers/csv_parser.h"
#include "parsers/json_parser.h"
#include "database/db_manager.h"
#include "models/table_schema.h"
#include "models/data_row.h"
#include <filesystem>
#include <sqlite3.h>
#include <unordered_map>
#include <vector>

static const std::filesystem::path TEST_DATA = TEST_DATA_DIR;



// ====================================================================== //
//  Fixture                                                               //
// ====================================================================== //

class IntegrationTest : public ::testing::Test {
protected:
    std::filesystem::path db_path;

    void SetUp() override {
        db_path = std::filesystem::temp_directory_path() / "test_integration.db";
        std::filesystem::remove(db_path);
    }

    void TearDown() override {
        std::filesystem::remove(db_path);
    }

    // Читает все строки таблицы - возвращает вектор map<column, value>
    std::vector<std::unordered_map<std::string, std::string>> read_table(
        const std::string& table_name)
    {
        sqlite3* db = nullptr;
        sqlite3_open(db_path.string().c_str(), &db);

        const std::string sql = "SELECT * FROM \"" + table_name + "\"";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        std::vector<std::unordered_map<std::string, std::string>> rows;
        const int col_count = sqlite3_column_count(stmt);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::unordered_map<std::string, std::string> row;
            for (int i = 0; i < col_count; ++i) {
                const char* col_name = sqlite3_column_name(stmt, i);
                const char* value    = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i));
                row[col_name] = value ? value : "";
            }
            rows.push_back(std::move(row));
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return rows;
    }

    int count_tables() {
        sqlite3* db = nullptr;
        sqlite3_open(db_path.string().c_str(), &db);
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table'",
            -1, &stmt, nullptr);
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return count;
    }

    bool table_exists(const std::string& name) {
        sqlite3* db = nullptr;
        sqlite3_open(db_path.string().c_str(), &db);
        sqlite3_stmt* stmt = nullptr;
        const std::string sql =
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" + name + "'";
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return count > 0;
    }
};



// ====================================================================== //
//  1. CSV -> SQLite                                                       //
// ====================================================================== //

TEST_F(IntegrationTest, CsvBasicRoundtrip) {
    auto parsed = parsers::csv_parser::parse(TEST_DATA / "basic.csv");

    database::db_manager db(db_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    const auto rows = read_table("basic");
    ASSERT_EQ(rows.size(), 3u);

    EXPECT_EQ(rows[0].at("name"), "Alice");
    EXPECT_EQ(rows[0].at("age"),  "30");
    EXPECT_EQ(rows[1].at("name"), "Bob");
    EXPECT_EQ(rows[2].at("name"), "Charlie");
}

TEST_F(IntegrationTest, CsvTypesPreserved) {
    auto parsed = parsers::csv_parser::parse(TEST_DATA / "basic.csv");

    database::db_manager db(db_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    const auto rows = read_table("basic");
    // score - REAL, SQLite возвращает как строку
    EXPECT_EQ(rows[0].at("score"), "9.5");
    EXPECT_EQ(rows[1].at("score"), "8.0");
}

TEST_F(IntegrationTest, CsvNullValues) {
    auto parsed = parsers::csv_parser::parse(TEST_DATA / "nulls.csv");

    database::db_manager db(db_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    const auto rows = read_table("nulls");
    ASSERT_EQ(rows.size(), 3u);

    EXPECT_EQ(rows[1].at("email"), "");
    EXPECT_EQ(rows[1].at("age"),   "");
}

TEST_F(IntegrationTest, CsvBooleanValues) {
    auto parsed = parsers::csv_parser::parse(TEST_DATA / "booleans.csv");

    database::db_manager db(db_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    const auto rows = read_table("booleans");
    EXPECT_EQ(rows[0].at("active"), "1");
    EXPECT_EQ(rows[1].at("active"), "0");
}

TEST_F(IntegrationTest, CsvQuotedFields) {
    auto parsed = parsers::csv_parser::parse(TEST_DATA / "quoted.csv");

    database::db_manager db(db_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    const auto rows = read_table("quoted");
    EXPECT_EQ(rows[0].at("description"), "Hello, world");
    EXPECT_EQ(rows[1].at("description"), "He said \"hi\"");
}

TEST_F(IntegrationTest, CsvRowCount) {
    auto parsed = parsers::csv_parser::parse(TEST_DATA / "basic.csv");

    database::db_manager db(db_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    const auto rows = read_table("basic");
    EXPECT_EQ(rows.size(), parsed.rows.size());
}



// ====================================================================== //
//  2. JSON -> SQLite                                                      //
// ====================================================================== //

TEST_F(IntegrationTest, JsonBasicRoundtrip) {
    auto parsed = parsers::json_parser::parse(TEST_DATA / "basic.json");

    database::db_manager db(db_path);
    for (const auto& table : parsed.tables) {
        db.create_table(table.schema);
    }

    std::unordered_map<std::string, std::vector<int64_t>> rowids;
    for (const auto& table : parsed.tables) {
        std::vector<int64_t> table_rowids;
        db.begin_transaction();
        try {
            for (std::size_t i = 0; i < table.rows.size(); ++i) {
                models::data_row row = table.rows[i];
                if (table.schema.is_child() && !table.parent_indices.empty()) {
                    const int64_t parent_idx = table.parent_indices[i];
                    const auto& parent_rowids = rowids.at(table.schema.parent_table());
                    row.set(table.schema.foreign_key(),
                            std::to_string(parent_rowids[parent_idx]));
                }
                db.insert_row(table.schema, row);
                table_rowids.push_back(db.last_insert_rowid());
            }
            db.commit();
        } catch (...) {
            db.rollback();
            throw;
        }
        rowids[table.schema.name()] = std::move(table_rowids);
    }

    const auto rows = read_table("basic");
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].at("name"), "Alice");
    EXPECT_EQ(rows[1].at("name"), "Bob");
    EXPECT_EQ(rows[2].at("name"), "Charlie");
}

TEST_F(IntegrationTest, JsonNestedArrayTwoTables) {
    auto parsed = parsers::json_parser::parse(TEST_DATA / "nested_array.json");

    database::db_manager db(db_path);
    for (const auto& table : parsed.tables) {
        db.create_table(table.schema);
    }

    std::unordered_map<std::string, std::vector<int64_t>> rowids;
    for (const auto& table : parsed.tables) {
        std::vector<int64_t> table_rowids;
        db.begin_transaction();
        try {
            for (std::size_t i = 0; i < table.rows.size(); ++i) {
                models::data_row row = table.rows[i];
                if (table.schema.is_child() && !table.parent_indices.empty()) {
                    const int64_t parent_idx    = table.parent_indices[i];
                    const auto& parent_rowids   = rowids.at(table.schema.parent_table());
                    row.set(table.schema.foreign_key(),
                            std::to_string(parent_rowids[parent_idx]));
                }
                db.insert_row(table.schema, row);
                table_rowids.push_back(db.last_insert_rowid());
            }
            db.commit();
        } catch (...) {
            db.rollback();
            throw;
        }
        rowids[table.schema.name()] = std::move(table_rowids);
    }

    // Обе таблицы созданы
    EXPECT_TRUE(table_exists("nested_array"));
    EXPECT_TRUE(table_exists("nested_array_orders"));

    // Родительская таблица - 2 строки
    EXPECT_EQ(read_table("nested_array").size(), 2u);

    // Дочерняя таблица - 3 строки
    EXPECT_EQ(read_table("nested_array_orders").size(), 3u);
}

TEST_F(IntegrationTest, JsonNestedArrayFkCorrect) {
    auto parsed = parsers::json_parser::parse(TEST_DATA / "nested_array.json");

    database::db_manager db(db_path);
    for (const auto& table : parsed.tables) {
        db.create_table(table.schema);
    }

    std::unordered_map<std::string, std::vector<int64_t>> rowids;
    for (const auto& table : parsed.tables) {
        std::vector<int64_t> table_rowids;
        db.begin_transaction();
        try {
            for (std::size_t i = 0; i < table.rows.size(); ++i) {
                models::data_row row = table.rows[i];
                if (table.schema.is_child() && !table.parent_indices.empty()) {
                    const int64_t parent_idx    = table.parent_indices[i];
                    const auto& parent_rowids   = rowids.at(table.schema.parent_table());
                    row.set(table.schema.foreign_key(),
                            std::to_string(parent_rowids[parent_idx]));
                }
                db.insert_row(table.schema, row);
                table_rowids.push_back(db.last_insert_rowid());
            }
            db.commit();
        } catch (...) {
            db.rollback();
            throw;
        }
        rowids[table.schema.name()] = std::move(table_rowids);
    }

    const auto orders = read_table("nested_array_orders");
    ASSERT_EQ(orders.size(), 3u);

    // Alice (id=1) -> первые два заказа должны иметь nested_array_id=1
    EXPECT_EQ(orders[0].at("nested_array_id"), "1");
    EXPECT_EQ(orders[1].at("nested_array_id"), "1");

    // Bob (id=2) -> третий заказ должен иметь nested_array_id=2
    EXPECT_EQ(orders[2].at("nested_array_id"), "2");
}

TEST_F(IntegrationTest, JsonPrimitiveArraySerialized) {
    auto parsed = parsers::json_parser::parse(TEST_DATA / "primitive_array.json");

    database::db_manager db(db_path);
    for (const auto& table : parsed.tables) {
        db.create_table(table.schema);
    }
    for (const auto& table : parsed.tables) {
        db.insert_rows(table.schema, table.rows);
    }

    const auto rows = read_table("primitive_array");
    EXPECT_EQ(rows[0].at("types"), "Grass,Poison");
    EXPECT_EQ(rows[1].at("types"), "Fire");
}