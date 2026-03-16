#include <gtest/gtest.h>
#include "database/db_manager.h"
#include "models/table_schema.h"
#include "models/data_row.h"
#include <filesystem>
#include <sqlite3.h>

using database::db_manager;
using database::db_exception;
using models::table_schema;
using models::data_row;
using models::Column;
using models::Sqlite_type;



// ====================================================================== //
//  Fixture                                                               //
// ====================================================================== //

class DbManagerTest : public ::testing::Test {
protected:
    std::filesystem::path db_path;

    void SetUp() override {
        db_path = std::filesystem::temp_directory_path() / "test_db_manager.db";
        std::filesystem::remove(db_path);
    }

    void TearDown() override {
        std::filesystem::remove(db_path);
    }

    // Вспомогательный метод - открывает базу напрямую через sqlite3 и выполняет запрос
    // Возвращает результат первой строки первой колонки
    std::string query_single(const std::string& sql) {
        sqlite3* db = nullptr;
        sqlite3_open(db_path.string().c_str(), &db);
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        std::string result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (text) result = text;
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }

    int query_count(const std::string& sql) {
        sqlite3* db = nullptr;
        sqlite3_open(db_path.string().c_str(), &db);
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        int result = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }

    // Стандартная схема для тестов
    table_schema make_simple_schema() {
        return table_schema("users", {
            {"name",  Sqlite_type::TEXT},
            {"age",   Sqlite_type::INTEGER},
            {"score", Sqlite_type::REAL}
        });
    }
};



// ====================================================================== //
//  1. Конструктор                                                        //
// ====================================================================== //

TEST_F(DbManagerTest, ConstructorCreatesFile) {
    {
        db_manager db(db_path);
    }
    EXPECT_TRUE(std::filesystem::exists(db_path));
}

TEST_F(DbManagerTest, ConstructorOpensExistingFile) {
    {
        db_manager db(db_path);
    }
    // Повторное открытие не бросает исключение
    EXPECT_NO_THROW(db_manager db(db_path));
}

TEST_F(DbManagerTest, MoveConstructor) {
    db_manager db1(db_path);
    db_manager db2 = std::move(db1);
    // db2 должен работать после перемещения
    EXPECT_NO_THROW(db2.create_table(make_simple_schema()));
}



// ====================================================================== //
//  2. create_table                                                       //
// ====================================================================== //

TEST_F(DbManagerTest, CreateTableBasic) {
    db_manager db(db_path);
    EXPECT_NO_THROW(db.create_table(make_simple_schema()));

    // Таблица существует в базе
    const std::string result = query_single(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='users'");
    EXPECT_EQ(result, "users");
}

TEST_F(DbManagerTest, CreateTableDropsExisting) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    // Вставляем строку
    data_row row;
    row.set("name", "Alice");
    row.set("age", "30");
    row.set("score", "9.5");
    db.insert_row(make_simple_schema(), row);

    // Пересоздаём таблицу - данные должны исчезнуть
    db.create_table(make_simple_schema());
    const int count = query_count("SELECT COUNT(*) FROM users");
    EXPECT_EQ(count, 0);
}

TEST_F(DbManagerTest, CreateTableWithAutoincrement) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    // PK колонка - AUTOINCREMENT
    const std::string ddl = query_single(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='users'");
    EXPECT_NE(ddl.find("AUTOINCREMENT"), std::string::npos);
}

TEST_F(DbManagerTest, CreateTableWithCustomPk) {
    table_schema schema("products", {
        {"name",  Sqlite_type::TEXT},
        {"price", Sqlite_type::REAL}
    }, "id", true);

    db_manager db(db_path);
    db.create_table(schema);

    const std::string ddl = query_single(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='products'");
    EXPECT_NE(ddl.find("PRIMARY KEY"), std::string::npos);
    // Без AUTOINCREMENT
    EXPECT_EQ(ddl.find("AUTOINCREMENT"), std::string::npos);
}

TEST_F(DbManagerTest, CreateChildTable) {
    // Родительская таблица
    table_schema parent("users", {{"name", Sqlite_type::TEXT}});
    // Дочерняя таблица
    table_schema child("orders", {{"amount", Sqlite_type::REAL}},
                       "id", false, "users", "users_id");

    db_manager db(db_path);
    db.create_table(parent);
    db.create_table(child);

    const std::string ddl = query_single(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='orders'");
    EXPECT_NE(ddl.find("REFERENCES"), std::string::npos);
    EXPECT_NE(ddl.find("users_id"),   std::string::npos);
}



// ====================================================================== //
//  3. insert_row                                                         //
// ====================================================================== //

TEST_F(DbManagerTest, InsertRowBasic) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    data_row row;
    row.set("name",  "Alice");
    row.set("age",   "30");
    row.set("score", "9.5");
    EXPECT_NO_THROW(db.insert_row(make_simple_schema(), row));

    EXPECT_EQ(query_count("SELECT COUNT(*) FROM users"), 1);
}

TEST_F(DbManagerTest, InsertRowValues) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    data_row row;
    row.set("name",  "Alice");
    row.set("age",   "30");
    row.set("score", "9.5");
    db.insert_row(make_simple_schema(), row);

    EXPECT_EQ(query_single("SELECT name  FROM users WHERE id=1"), "Alice");
    EXPECT_EQ(query_single("SELECT age   FROM users WHERE id=1"), "30");
    EXPECT_EQ(query_single("SELECT score FROM users WHERE id=1"), "9.5");
}

TEST_F(DbManagerTest, InsertRowWithNull) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    data_row row;
    row.set("name", "Alice");
    row.set_null("age");
    row.set_null("score");
    db.insert_row(make_simple_schema(), row);

    // NULL значение - sqlite3_column_text вернёт пустую строку
    EXPECT_EQ(query_single("SELECT age FROM users WHERE id=1"), "");
}

TEST_F(DbManagerTest, InsertMultipleRows) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    for (int i = 0; i < 5; ++i) {
        data_row row;
        row.set("name",  "User" + std::to_string(i));
        row.set("age",   std::to_string(20 + i));
        row.set("score", "7.0");
        db.insert_row(make_simple_schema(), row);
    }

    EXPECT_EQ(query_count("SELECT COUNT(*) FROM users"), 5);
}



// ====================================================================== //
//  4. insert_rows                                                        //
// ====================================================================== //

TEST_F(DbManagerTest, InsertRowsBatch) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    std::vector<data_row> rows;
    for (int i = 0; i < 10; ++i) {
        data_row row;
        row.set("name",  "User" + std::to_string(i));
        row.set("age",   std::to_string(20 + i));
        row.set("score", "8.0");
        rows.push_back(std::move(row));
    }

    EXPECT_NO_THROW(db.insert_rows(make_simple_schema(), rows));
    EXPECT_EQ(query_count("SELECT COUNT(*) FROM users"), 10);
}



// ====================================================================== //
//  5. Транзакции                                                         //
// ====================================================================== //

TEST_F(DbManagerTest, TransactionCommit) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    db.begin_transaction();
    data_row row;
    row.set("name", "Alice");
    row.set("age",  "30");
    row.set("score", "9.5");
    db.insert_row(make_simple_schema(), row);
    db.commit();

    EXPECT_EQ(query_count("SELECT COUNT(*) FROM users"), 1);
}

TEST_F(DbManagerTest, TransactionRollback) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    db.begin_transaction();
    data_row row;
    row.set("name", "Alice");
    row.set("age",  "30");
    row.set("score", "9.5");
    db.insert_row(make_simple_schema(), row);
    db.rollback();

    EXPECT_EQ(query_count("SELECT COUNT(*) FROM users"), 0);
}



// ====================================================================== //
//  6. last_insert_rowid                                                  //
// ====================================================================== //

TEST_F(DbManagerTest, LastInsertRowid) {
    db_manager db(db_path);
    db.create_table(make_simple_schema());

    data_row row;
    row.set("name",  "Alice");
    row.set("age",   "30");
    row.set("score", "9.5");
    db.insert_row(make_simple_schema(), row);

    EXPECT_EQ(db.last_insert_rowid(), 1);

    data_row row2;
    row2.set("name",  "Bob");
    row2.set("age",   "25");
    row2.set("score", "8.0");
    db.insert_row(make_simple_schema(), row2);

    EXPECT_EQ(db.last_insert_rowid(), 2);
}