#include "data_row.h"

namespace models {

    data_row::data_row(Storage data) : data_(std::move(data)) {}



    // ------------------------------------------------------------------ //
    //  Запись                                                            //
    // ------------------------------------------------------------------ //

    void data_row::set(const std::string& column, Value value) {
        data_[column] = std::move(value);
    }


    void data_row::set_null(const std::string& column) {
        data_[column] = std::nullopt;
    }




    // ------------------------------------------------------------------ //
    //  Чтение                                                            //
    // ------------------------------------------------------------------ //

    data_row::Value data_row::get(const std::string& column) const {
        Storage::const_iterator it = data_.find(column);
        if (it == data_.end()) {
            return std::nullopt;
        }
        return it->second;
    }


    bool data_row::has(const std::string& column) const {
        return data_.contains(column);
    }


    bool data_row::is_null(const std::string& column) const {
        Storage::const_iterator it = data_.find(column);
        if (it == data_.end()) {
            return true;
        }
        return !it->second.has_value();
    }


    std::size_t data_row::size() const {
        return data_.size();
    }


    bool data_row::empty() const {
        return data_.empty();
    }



    // ------------------------------------------------------------------ //
    //  Итерация                                                          //
    // ------------------------------------------------------------------ //

    data_row::Storage::const_iterator data_row::begin() const {
        return data_.begin();
    }


    data_row::Storage::const_iterator data_row::end() const {
        return data_.end();
    }

}