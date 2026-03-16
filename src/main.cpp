#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include "parsers/csv_parser.h"
#include "parsers/json_parser.h"
#include "database/db_manager.h"

namespace fs = std::filesystem;








// ====================================================================== //
//  Вспомогательные функции                                               //
// ====================================================================== //

static void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <input.csv|input.json> [output.db]\n"
              << "Example: " << program_name << " data.csv result.db\n"
              << "         " << program_name << " data.json result.db\n";
}


static fs::path resolve_output_path(const fs::path& input, const fs::path& output) {
    if (output.empty()) {
        return input.parent_path() / (input.stem().string() + ".db");
    }
    return output;
}


static std::string to_lower_ext(const fs::path& path) {
    std::string ext = path.extension().string();
    for (char& c : ext) c = std::tolower(static_cast<unsigned char>(c));
    return ext;
}



// ====================================================================== //
//  Обработка CSV                                                         //
// ====================================================================== //

static void process_csv(const fs::path& input_path, const fs::path& output_path) {
    std::cout << "Parsing '" << input_path.string() << "'...\n";
    parsers::csv_parse_result parsed = parsers::csv_parser::parse(input_path);
    std::cout << "Parsed " << parsed.rows.size() << " rows, "
              << parsed.schema.column_count() << " columns.\n";

    std::cout << "Writing to '" << output_path.string() << "'...\n";
    database::db_manager db(output_path);
    db.create_table(parsed.schema);
    db.insert_rows(parsed.schema, parsed.rows);

    std::cout << "Done. Table '" << parsed.schema.name()
              << "' created with " << parsed.rows.size() << " rows.\n";
}



// ====================================================================== //
//  Обработка JSON                                                        //
// ====================================================================== //

static void process_json(const fs::path& input_path, const fs::path& output_path) {
    std::cout << "Parsing '" << input_path.string() << "'...\n";
    parsers::json_parse_result parsed = parsers::json_parser::parse(input_path);
    std::cout << "Parsed " << parsed.tables.size() << " table(s).\n";

    database::db_manager db(output_path);

    // Сначала создаём все таблицы
    for (const auto& table : parsed.tables) {
        db.create_table(table.schema);
    }

    // Вставляем строки - для дочерних таблиц проставляем FK
    // rowids[table_name] = вектор rowid вставленных строк в порядке вставки
    std::unordered_map<std::string, std::vector<int64_t>> rowids;

    for (const auto& table : parsed.tables) {
        std::vector<int64_t> table_rowids;
        table_rowids.reserve(table.rows.size());

        db.begin_transaction();
        try {
            for (std::size_t i = 0; i < table.rows.size(); ++i) {
                models::data_row row = table.rows[i];

                // Если дочерняя - проставляем FK из rowid родителя
                if (table.schema.is_child() && !table.parent_indices.empty()) {
                    const int64_t parent_idx      = table.parent_indices[i];
                    const auto&   parent_rowids   = rowids.at(table.schema.parent_table());
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

        std::cout << "Table '" << table.schema.name()
                  << "': " << table.rows.size() << " rows.\n";
    }

    std::cout << "Done. Written to '" << output_path.string() << "'.\n";
}



// ====================================================================== //
//  main                                                                  //
// ====================================================================== //

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    const fs::path input_path  = argv[1];
    const fs::path output_path = resolve_output_path(
        input_path, argc == 3 ? fs::path(argv[2]) : fs::path{});
    const std::string ext = to_lower_ext(input_path);

    try {
        if (ext == ".csv") {
            process_csv(input_path, output_path);
        } else if (ext == ".json") {
            process_json(input_path, output_path);
        } else {
            std::cerr << "Error: unsupported file extension '" << ext << "'\n";
            print_usage(argv[0]);
            return 1;
        }

    } catch (const parsers::csv_parse_exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    } catch (const parsers::json_parse_exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    } catch (const database::db_exception& e) {
        std::cerr << "Database error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}