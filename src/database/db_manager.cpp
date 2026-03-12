#include "db_manager.h"
#include <sqlite3.h>

namespace database {

    // ================================================================== //
    //  Конструктор / Деструктор / Присваивание / Перемещение             //
    // ================================================================== //

    db_manager::db_manager(const std::filesystem::path& path) {
        const int rc = sqlite3_open(path.string().c_str(), &db_);

        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw db_exception("Cannot open database '" + path.string() + "': " + msg);
        }
    }


    db_manager::~db_manager() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }


    db_manager::db_manager(db_manager&& other) noexcept : db_(other.db_) {
        other.db_ = nullptr;
    }


    db_manager& db_manager::operator=(db_manager&& other) noexcept {
        if (this != &other) {
            if (db_) {
                sqlite3_close(db_);
            }
            db_ = other.db_;
            other.db_ = nullptr;
        }

        return *this;
    }



    // ================================================================== //
    //  Вспомогательные методы                                            //
    // ================================================================== //

    void db_manager::execute(const std::string& sql_script) {
        char* err_msg = nullptr;
        const int rc = sqlite3_exec(db_, sql_script.c_str(), nullptr, nullptr, &err_msg);

        if (rc != SQLITE_OK) {
            const std::string msg = err_msg ? err_msg : "unknown error";
            sqlite3_free(err_msg);
            throw db_exception("SQL error: " + msg + "\nQuery: " + sql_script);
        }
    }


    void db_manager::bind_value(sqlite3_stmt* stmt, int index, const utils::sql_value& value) {
        int rc = std::visit([&](const auto& v) -> int {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return sqlite3_bind_null(stmt, index);

            } else if constexpr (std::is_same_v<T, int64_t>) {
                return sqlite3_bind_int64(stmt, index, v);

            } else if constexpr (std::is_same_v<T, double>) {
                return sqlite3_bind_double(stmt, index, v);

            } else if constexpr (std::is_same_v<T, bool>) {
                return sqlite3_bind_int(stmt, index, v ? 1 : 0);

            } else if constexpr (std::is_same_v<T, std::string>) {
                return sqlite3_bind_text(stmt, index, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
            } else {
                static_assert(false, "Unhandled sql_value alternative");
            }
        }, value);

        if (rc != SQLITE_OK) {
            throw db_exception(std::string("Failed to bind value: ") + sqlite3_errmsg(db_));
        }
    }


    std::string db_manager::build_create_table_sql(const models::table_schema& schema) {
        std::string sql = "CREATE TABLE ";
                          
        sql += utils::string_utils::quote_sql_identifier(schema.name());
        sql += " (\n";

        // PK колонка
        sql += "  ";
        sql += utils::string_utils::quote_sql_identifier(schema.pk_column());
        if (schema.has_custom_pk()) {
            sql += " INTEGER PRIMARY KEY,\n";
        } else {
            sql += " INTEGER PRIMARY KEY AUTOINCREMENT,\n";
        }

        // FK колонка если дочерняя таблица
        if (schema.is_child()) {
            sql += "  ";
            sql += utils::string_utils::quote_sql_identifier(schema.foreign_key());
            sql += " INTEGER NOT NULL REFERENCES ";
            sql += utils::string_utils::quote_sql_identifier(schema.parent_table());
            sql += ",\n";
        }

        // Остальные колонки
        const std::vector<models::Column>& col_from_schema = schema.columns();
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, col_from_schema.size())) {
            sql += "  ";
            sql += utils::string_utils::quote_sql_identifier(col_from_schema[i].name);
            sql += " ";
            switch (col_from_schema[i].type) {
                case models::Sqlite_type::INTEGER: sql += "INTEGER"; break;
                case models::Sqlite_type::REAL:    sql += "REAL";    break;
                case models::Sqlite_type::TEXT:    sql += "TEXT";    break;
            }
            sql += ",\n";
        }

        // Убираем последнюю запятую
        if (sql.back() == '\n') {
            sql.pop_back();
        }
        if (sql.back() == ',') {
            sql.pop_back();
        }
        sql += "\n);";

        return sql;
    }


    std::string db_manager::build_insert_sql(const models::table_schema& schema) {
        std::string sql = "INSERT INTO ";
        sql += utils::string_utils::quote_sql_identifier(schema.name());
        sql += " (";

        std::vector<std::string> col_names;

        if (schema.has_custom_pk()) {
            col_names.push_back(schema.pk_column());
        }

        if (schema.is_child()) {
            col_names.push_back(schema.foreign_key());
        }

        const std::vector<models::Column>& col_from_schema = schema.columns();
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, col_from_schema.size())) {
            col_names.push_back(col_from_schema[i].name);
        }

        // Имена колонок
         for (std::size_t i : std::ranges::iota_view(std::size_t{0}, col_names.size())) {
            sql += utils::string_utils::quote_sql_identifier(col_names[i]);
            if (i + 1 < col_names.size()) sql += ", ";
        }

        // Плейсхолдеры
        sql += ") VALUES (";
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, col_names.size())) {
            sql += "?";
            if (i + 1 < col_names.size()) {
                sql += ", ";
            }
        }
        sql += ");";

        return sql;
    }



    // ================================================================== //
    //  Транзакции                                                        //
    // ================================================================== //

    void db_manager::begin_transaction() {
        execute("BEGIN TRANSACTION");
    }


    void db_manager::commit() {
        execute("COMMIT");
    }


    void db_manager::rollback() {
        execute("ROLLBACK");
    }



    // ================================================================== //
    //  DDL                                                               //
    // ================================================================== //

    void db_manager::create_table(const models::table_schema& schema) {
        execute("DROP TABLE IF EXISTS " + utils::string_utils::quote_sql_identifier(schema.name()));
        execute(build_create_table_sql(schema));
    }



    // ================================================================== //
    //  Вставка                                                           //
    // ================================================================== //

    void db_manager::insert_row(const models::table_schema& schema, const models::data_row& row) {
        std::string sql = build_insert_sql(schema);

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            throw db_exception(std::string("Failed to prepare statement: ") + sqlite3_errmsg(db_));
        }

        // RAII для stmt — финализируем в любом случае
        struct stmt_guard {
            sqlite3_stmt* s;
            ~stmt_guard() { 
                sqlite3_finalize(s); 
            }
        } guard{stmt};

        // Биндим значения
        int bind_index = 1; // SQLite индексы начинаются с 1

        if (schema.has_custom_pk()) {
            models::data_row::Value pk_val = row.get(schema.pk_column());
            
            if (!pk_val.has_value()) {
                throw db_exception(
                    "Primary key '" + schema.pk_column() + "' is NULL in row");
            }
            
            utils::sql_value val = utils::type_converter::convert(*pk_val, utils::sql_type::Integer);
            bind_value(stmt, bind_index++, val);
        }

        if (schema.is_child()) {
            models::data_row::Value fk_val = row.get(schema.foreign_key());
            if (!fk_val.has_value()) {
                throw db_exception("Foreign key '" + schema.foreign_key() + "' is NULL in row");
            }
            
            utils::sql_value val = utils::type_converter::convert(*fk_val, utils::sql_type::Integer);
            bind_value(stmt, bind_index++, val);
        }

        const std::vector<models::Column>& col_from_schema = schema.columns();
        for (std::size_t i : std::ranges::iota_view(std::size_t{0}, col_from_schema.size())) {
            models::data_row::Value raw = row.get(col_from_schema[i].name);

            utils::sql_type col_type;
            switch (col_from_schema[i].type) {
                case models::Sqlite_type::INTEGER: col_type = utils::sql_type::Integer; break;
                case models::Sqlite_type::REAL:    col_type = utils::sql_type::Real;    break;
                case models::Sqlite_type::TEXT:    col_type = utils::sql_type::Text;    break;
            }

            utils::sql_value val = raw.has_value()
                ? utils::type_converter::convert(*raw, col_type) : utils::sql_value(nullptr);

            bind_value(stmt, bind_index++, val);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            throw db_exception(std::string("Failed to insert row: ") + sqlite3_errmsg(db_));
        }
    }


    void db_manager::insert_rows(const models::table_schema& schema, const std::vector<models::data_row>& rows) {
        begin_transaction();
        try {
            for (std::size_t i : std::ranges::iota_view(std::size_t{0}, rows.size())) {
                 insert_row(schema, rows[i]);
            }
            commit();
        } catch (...) {
            rollback();
            throw;
        }
    }

} 