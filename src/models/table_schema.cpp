#include "table_schema.h"
#include <algorithm>

namespace models {

    // ========================================================================== //
    //  Конструкторы                                                               //
    // ========================================================================== //

    table_schema::table_schema(std::string name,
                            std::vector<Column> columns,
                            std::string pk_column,
                            bool has_custom_pk)
        : name_(std::move(name))
        , columns_(std::move(columns))
        , pk_column_(std::move(pk_column))
        , has_custom_pk_(has_custom_pk)
        , parent_table_{}
        , foreign_key_{} {}

        
    table_schema::table_schema(std::string name,
                            std::vector<Column> columns,
                            std::string pk_column,
                            bool has_custom_pk,
                            std::string parent_table,
                            std::string foreign_key)
        : name_(std::move(name))
        , columns_(std::move(columns))
        , pk_column_(std::move(pk_column))
        , has_custom_pk_(has_custom_pk)
        , parent_table_(std::move(parent_table))
        , foreign_key_(std::move(foreign_key)) {}



    // ========================================================================== //
    //  Основные свойства                                                         //
    // ========================================================================== //

    const std::string& table_schema::name() const {
        return name_;
    }


    const std::vector<Column>& table_schema::columns() const {
        return columns_;
    }


    const std::string& table_schema::pk_column() const {
        return pk_column_;
    }


    bool table_schema::has_custom_pk() const {
        return has_custom_pk_;
    }



    // ========================================================================== //
    //  Связи                                                                     //
    // ========================================================================== //

    bool table_schema::is_child() const {
        return !parent_table_.empty();
    }


    const std::string& table_schema::parent_table() const {
        return parent_table_;
    }


    const std::string& table_schema::foreign_key() const {
        return foreign_key_;
    }



    // ========================================================================== //
    //  Поиск колонок                                                             //
    // ========================================================================== //

    std::optional<Column> table_schema::find_column(const std::string& name) const {
        const auto it = std::ranges::find_if(columns_,
            [&name](const Column& col) { return col.name == name; });

        if (it == columns_.end()) {
            return std::nullopt;
        }
        return std::make_optional(*it);
    }


    bool table_schema::has_column(const std::string& name) const {
        return std::ranges::any_of(columns_,
            [&name](const Column& col) { return col.name == name; });
    }


    std::size_t table_schema::column_count() const {
        return columns_.size();
    }

} 