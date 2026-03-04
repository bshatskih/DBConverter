#include "file_validator.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>



namespace utils {

    file_validator::file_validator(uintmax_t max_file_size) : 
        max_file_size_(max_file_size) {}

    // -------------------------------------------------------------------------- //
    //  Основной метод                                                             //
    // -------------------------------------------------------------------------- //

    validation_result file_validator::validate(const std::filesystem::path& path) const {
        validation_result result;

        result = check_path_not_empty(path);
        if (!result) return result;

        result = check_exists(path);
        if (!result) return result;

        result = check_is_regular_file(path);
        if (!result) return result;
            
        result = check_readable(path);    
        if (!result) return result;
            
        result = this->check_size(path);   
        if (!result) return result;
            
        result = check_extension(path);    
        if (!result) return result;
            
        result = check_not_binary(path);   
        if (!result)  return result;

        return validation_result::ok();
    }




    // ------------------------------------------------------------------ //
    //  Атомарные проверки                                                //
    // ------------------------------------------------------------------ //


    validation_result file_validator::check_path_not_empty(const std::filesystem::path& path) {
        if (path.empty()) {
            return validation_result::fail("File path is empty");
        }
        return validation_result::ok();
    }


    validation_result file_validator::check_exists(const std::filesystem::path& path) {
        std::error_code ec;

        if (!std::filesystem::exists(path, ec)) {
            if (ec) {
                return validation_result::fail(
                    "Cannot access path '" + path.string() + "': " + ec.message());
            }
            return validation_result::fail(
                "File does not exist: '" + path.string() + "'");
        }
        return validation_result::ok();
    }


    validation_result file_validator::check_is_regular_file(const std::filesystem::path& path) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            if (ec) {
                return validation_result::fail(
                    "Cannot determine file type for '" + path.string() + "': " + ec.message());
            }
            return validation_result::fail(
                "'" + path.string() + "' is not a regular file (it may be a directory, symlink, or device)");
        }
        return validation_result::ok();
    }


    validation_result file_validator::check_readable(const std::filesystem::path& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            const char* reason = std::strerror(errno);
            return validation_result::fail(
                "Cannot open file '" + path.string() + "' for reading: " +
                (reason ? reason : "unknown error"));
        }
        return validation_result::ok();
    }


    validation_result file_validator::check_size(const std::filesystem::path& path) const {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);

        if (ec) {
            return validation_result::fail(
                "Cannot determine size of '" + path.string() + "': " + ec.message());
        }

        if (size == 0) {
            return validation_result::fail(
                "File is empty: '" + path.string() + "'");
        }
        if (size > max_file_size_) {
            return validation_result::fail(
                "File '" + path.string() + "' is too large (" +
                std::to_string(size) + " bytes); maximum allowed is " +
                std::to_string(max_file_size_) + " bytes");
        }
        return validation_result::ok();
    }


    validation_result file_validator::check_extension(const std::filesystem::path& path) {
        if (detect_type(path) == file_type::UNKNOWN) {
            return validation_result::fail(
                "Unsupported file extension '" + path.extension().string() +
                "' for file '" + path.filename().string() + "'. "
                "Expected .csv or .json");
        }
        return validation_result::ok();
    }

    
    validation_result file_validator::check_not_binary(const std::filesystem::path& path) {
        std::ifstream bin_fin(path, std::ios::binary);
        
        if (!bin_fin.is_open()) {
            return validation_result::fail(
                "Cannot open file for binary check: '" + path.string() + "'");
        }

        std::array<char, BINARY_PROBE_SIZE> buf{};
        bin_fin.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::size_t read_bytes = static_cast<std::size_t>(bin_fin.gcount());

        if (read_bytes == 0) {
            return validation_result::ok();
        }

        if (looks_binary(buf.data(), read_bytes)) {
            return validation_result::fail(
                "File appears to be binary, not a text file: '" + path.string() + "'");
        }
        return validation_result::ok();
    }


    




    // -------------------------------------------------------------------------- //
    //  Вспомогательные методы                                                    //
    // -------------------------------------------------------------------------- //

    file_type file_validator::detect_type(const std::filesystem::path& path) {
        std::string file_extension = path.extension().string();
        for (char& c : file_extension) {
            c = std::tolower(c);
        }

        if (file_extension == ".csv")  
            return file_type::CSV;
        if (file_extension == ".json") 
            return file_type::JSON;

        return file_type::UNKNOWN;
    }


    bool file_validator::looks_binary(const char* buf, std::size_t buf_len) {
        constexpr double MAX_CONTROL_RATIO = 0.30; 
        std::size_t control_count = 0;

        for (std::size_t i = 0; i < buf_len; ++i) {
            char cur_byte = buf[i];

            if (cur_byte == 0x00) {
                return true;
            }

            bool is_allowed_control =
                (cur_byte == '\t') ||  
                (cur_byte == '\n') ||  
                (cur_byte == '\r') ||  
                (cur_byte == 0x1B);    // ESC символ для обозначения начала управляющей последовательности

            // cur_byte < 0x20 - первые 32 символа ASCII - управляющие символы
            if (cur_byte < 0x20 && !is_allowed_control) {
                ++control_count;
            }
        }

        return (static_cast<double>(control_count) / buf_len) > MAX_CONTROL_RATIO;
    }

}

