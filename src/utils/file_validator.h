#pragma once
#include <string>
#include <filesystem>
#include <system_error>

namespace utils {

    // ------------------------------------------------------------------ //
    // Результат валидации файла                                          //
    // ------------------------------------------------------------------ //
    struct validation_result {
        bool valid = false;
        std::string error;     

        explicit operator bool() const { return valid; }

        static validation_result ok() {
            return {true, {}};
        }

        static validation_result fail(std::string message) {
            return {false, std::move(message)};
        }
    };

    // ------------------------------------------------------------------ //
    // Поддерживаемые типы файлов                                         //
    // ------------------------------------------------------------------ //
    enum class file_type {
        CSV,
        JSON,
        UNKNOWN
    };





    class file_validator {
     private:
        uintmax_t max_file_size_;

        
     public:
        static constexpr uintmax_t DEFAULT_MAX_FILE_SIZE = 512ULL * 1024 * 1024;
        static constexpr std::size_t BINARY_PROBE_SIZE = 512;

        explicit file_validator(uintmax_t max_file_size = DEFAULT_MAX_FILE_SIZE);

        // ------------------------------------------------------------------ //
        //  Основной метод                                                     //
        // ------------------------------------------------------------------ //
        [[nodiscard]] validation_result validate(const std::filesystem::path& path) const;




        // ------------------------------------------------------------------ //
        //  Атомарные проверки                                                //
        // ------------------------------------------------------------------ //
        [[nodiscard]] static validation_result check_path_not_empty(const std::filesystem::path& path);
        [[nodiscard]] static validation_result check_exists(const std::filesystem::path& path);
        [[nodiscard]] static validation_result check_is_regular_file(const std::filesystem::path& path);
        [[nodiscard]] static validation_result check_readable(const std::filesystem::path& path);
        [[nodiscard]] validation_result check_size(const std::filesystem::path& path) const;
        [[nodiscard]] static validation_result check_extension(const std::filesystem::path& path);
        [[nodiscard]] static validation_result check_not_binary(const std::filesystem::path& path);



        // ------------------------------------------------------------------ //
        //  Вспомогательные утилиты                                           //
        // ------------------------------------------------------------------ //
        [[nodiscard]] static file_type detect_type(const std::filesystem::path& path);
        [[nodiscard]] static bool looks_binary(const char* buf, std::size_t len);
    };
} 