#pragma once
#include <string>
#include <vector>
#include <optional>

namespace models {

    // ========================================================================== //
    //  Sqlite_type                                                               //
    // ========================================================================== //

    enum class Sqlite_type {
        INTEGER,
        REAL,
        TEXT
    };

    // ========================================================================== //
    //  Column                                                                    //
    // ========================================================================== //

    struct Column {
        std::string name;
        Sqlite_type  type;
    };

    // ========================================================================== //
    //  table_schema                                                              //
    // ========================================================================== //

    class table_schema {
        std::string          name_;
        std::vector<Column>  columns_;
        std::string          pk_column_;
        bool                 has_custom_pk_;
        std::string          parent_table_;
        std::string          foreign_key_;
     public:

        table_schema(std::string name,
                    std::vector<Column> columns,
                    std::string pk_column = "id",
                    bool has_custom_pk = false);

        table_schema(std::string name,
                    std::vector<Column> columns,
                    std::string pk_column,
                    bool has_custom_pk,
                    std::string parent_table,
                    std::string foreign_key);



        // ------------------------------------------------------------------ //
        //  Основные свойства                                                 //
        // ------------------------------------------------------------------ //

        /// Имя таблицы
        [[nodiscard]] const std::string& name() const;

        /// Список колонок (без PK и FK - они хранятся отдельно)
        [[nodiscard]] const std::vector<Column>& columns() const;

        /// Имя PK колонки
        [[nodiscard]] const std::string& pk_column() const;

        /// true если PK взят из данных, false если сгенерирован (AUTOINCREMENT)
        [[nodiscard]] bool has_custom_pk() const;



        // ------------------------------------------------------------------ //
        //  Связи                                                             //
        // ------------------------------------------------------------------ //

        /// true если таблица является дочерней (имеет родителя)
        [[nodiscard]] bool is_child() const;

        /// Имя родительской таблицы. Пусто если таблица корневая.
        [[nodiscard]] const std::string& parent_table() const;

        /// Имя FK колонки в этой таблице (например "user_id"). Пусто если корневая.
        [[nodiscard]] const std::string& foreign_key() const;



        // ------------------------------------------------------------------ //
        //  Поиск колонок                                                     //
        // ------------------------------------------------------------------ //

        /// Возвращает колонку по имени. std::nullopt если колонка не найдена.
        [[nodiscard]] std::optional<Column> find_column(const std::string& name) const;

        /// true если колонка с таким именем существует
        [[nodiscard]] bool has_column(const std::string& name) const;

        /// Количество колонок
        [[nodiscard]] std::size_t column_count() const;
    };

}