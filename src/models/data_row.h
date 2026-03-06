#pragma once
#include <string>
#include <optional>
#include <unordered_map>



namespace models {

    class data_row {
     public:
        using Value = std::optional<std::string>;
        using Storage = std::unordered_map<std::string, Value>;

     private:
        Storage data_;

     public:

        data_row() = default;
        explicit data_row(Storage data);

        // ------------------------------------------------------------------ //
        //  Запись                                                            //
        // ------------------------------------------------------------------ //

        // Устанавливает значение колонки
        void set(const std::string& column, Value value);

        // Устанавливает NULL для колонки
        void set_null(const std::string& column);



        // ------------------------------------------------------------------ //
        //  Чтение                                                              //
        // ------------------------------------------------------------------ //

        // Возвращает значение колонки.
        // Если колонка отсутствует - возвращает std::nullopt (трактуется как NULL).
        [[nodiscard]] Value get(const std::string& column) const;

        // true если колонка присутствует в строке (даже если значение NULL)
        [[nodiscard]] bool has(const std::string& column) const;

        // true если значение колонки равно NULL (std::nullopt)
        [[nodiscard]] bool is_null(const std::string& column) const;

        // Количество колонок в строке
        [[nodiscard]] std::size_t size() const;

        // true если строка не содержит ни одной колонки
        [[nodiscard]] bool empty() const;



        // ------------------------------------------------------------------ //
        //  Итерация                                                          //
        // ------------------------------------------------------------------ //

        [[nodiscard]] Storage::const_iterator begin() const;
        [[nodiscard]] Storage::const_iterator end() const;
    };

} 