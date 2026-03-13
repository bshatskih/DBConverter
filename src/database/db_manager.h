#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <ranges>
#include <stdexcept>
#include "../models/table_schema.h"
#include "../models/data_row.h"
#include "../utils/type_converter.h"
#include "../utils/string_utils.h"
#include <sqlite3.h>

namespace database {

    // ================================================================== //
    //  Исключение                                                        //
    // ================================================================== //

    class db_exception : public std::runtime_error {
    public:
        explicit db_exception(const std::string& message)
            : std::runtime_error(message) {}
    };






    class db_manager {
     private:
        sqlite3* db_ = nullptr;


        // ================================================================== //
        //  Вспомогательные методы                                            //
        // ================================================================== //

        // Для выполнения SQL-скриптов без результата (DDL, BEGIN/COMMIT/ROLLBACK)
        void execute(const std::string& sql);

        // При связывании значения с sqlite3_stmt
        void bind_value(sqlite3_stmt* stmt, int index, const utils::sql_value& value);
        
        // Генерирует SQL для создания таблицы по схеме
        std::string build_create_table_sql(const models::table_schema& schema);
        
        // Генерирует SQL для вставки строки по схеме
        std::string build_insert_sql(const models::table_schema& schema);

     public:

        // ================================================================== //
        //  Конструктор / Деструктор / Присваивание / Перемещение             //
        // ================================================================== //
     
        explicit db_manager(const std::filesystem::path& path);
        
        ~db_manager();

        // Удаляем копирующие конструкторы и операторы, разрешаем только перемещение
        db_manager(const db_manager&) = delete;
        
        db_manager& operator=(const db_manager&) = delete;
        
        db_manager(db_manager&&) noexcept;

        db_manager& operator=(db_manager&&) noexcept;



        // ------------------------------------------------------------------ //
        //  Транзакции                                                        //
        // ------------------------------------------------------------------ //
        
        // Начинает транзакцию
        void begin_transaction();
        
        // Фиксирует транзакцию
        void commit();
        
        // Откатывает транзакцию
        void rollback();



        // ------------------------------------------------------------------ //
        //  DDL                                                               //
        // ------------------------------------------------------------------ //

        // Создаёт таблицу по схеме. Если таблица уже существует - удаляет её и создаёт заново.
        void create_table(const models::table_schema& schema);



        // ------------------------------------------------------------------ //
        //  Вставка                                                           //
        // ------------------------------------------------------------------ //

        // Вставляет одну строку. Транзакция на вызывающем коде.
        void insert_row(const models::table_schema& schema, const models::data_row& row);

        // Вставляет все строки в одной транзакции.
        // При ошибке делает rollback и пробрасывает исключение.
        void insert_rows(const models::table_schema& schema, const std::vector<models::data_row>& rows);
    
        // Возвращает rowid последней вставленной строки.
        // Используется для получения PK родительской строки при вставке дочерних.
        [[nodiscard]] int64_t last_insert_rowid() const;
            
    };

}