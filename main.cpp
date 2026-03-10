#include <iostream>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <string>
#include <cstdlib>

#include "crow.h"
#include "magic_args/magic_args.hpp"

struct CommandRegion {
    int x1, y1, x2, y2;
    std::string command;
    std::string label;
};

struct Page {
    std::string name;
    std::vector<CommandRegion> regions;
};

namespace {

struct AppOptions {
    std::optional<std::string> mPagesFile;
};

struct ParsedCommandLine {
    AppOptions options;
    int exit_code {0};
    bool should_exit {false};
};

ParsedCommandLine parse_command_line(int argc, char* argv[]) {
    auto const program_info = magic_args::program_info{
        .mDescription = "Run the touchpad web server with an optional pages JSON file.",
        .mExamples = {
            "crow_server",
            "crow_server --pages-file pages.example.json",
        },
    };

    auto const parsed = magic_args::parse<AppOptions>(argc, argv, program_info);
    if (parsed.has_value()) {
        return ParsedCommandLine{.options = *parsed};
    }

    switch (parsed.error()) {
        case magic_args::HelpRequested:
        case magic_args::VersionRequested:
            return ParsedCommandLine{.exit_code = 0, .should_exit = true};
        default:
            return ParsedCommandLine{.exit_code = 1, .should_exit = true};
    }
}

std::vector<Page> make_default_pages() {
    return {
        {
            "Media Control",
            {
                {0, 0, 500, 500, "echo 'Volume Up'", "Vol +"},
                {500, 0, 1000, 500, "echo 'Volume Down'", "Vol -"},
                {0, 500, 500, 1000, "echo 'Previous Track'", "Prev"},
                {500, 500, 1000, 1000, "echo 'Next Track'", "Next"}
            }
        },
        {
            "App Launchers",
            {
                {0, 0, 500, 500, "echo 'Opening Browser'", "Browser"},
                {500, 0, 1000, 500, "echo 'Opening Terminal'", "Terminal"},
                {0, 500, 500, 1000, "echo 'Opening Editor'", "Editor"},
                {500, 500, 1000, 1000, "echo 'Opening Files'", "Files"}
            }
        },
        {
            "System Commands",
            {
                {0, 0, 500, 500, "echo 'Lock Screen'", "Lock"},
                {500, 0, 1000, 500, "echo 'Sleep Mode'", "Sleep"},
                {0, 500, 500, 1000, "echo 'Restart'", "Restart"},
                {500, 500, 1000, 1000, "echo 'Shutdown'", "Power Off"}
            }
        }
    };
}

[[noreturn]] void throw_validation_error(const std::string& path, const std::string& message) {
    throw std::runtime_error(path + ": " + message);
}

void require_json_type(const crow::json::rvalue& value, crow::json::type expected, const std::string& path, const std::string& expected_name) {
    if (value.t() != expected) {
        throw_validation_error(path, "expected " + expected_name);
    }
}

std::string require_string(const crow::json::rvalue& value, const std::string& path) {
    require_json_type(value, crow::json::type::String, path, "string");
    return std::string{value.s()};
}

int require_int(const crow::json::rvalue& value, const std::string& path) {
    require_json_type(value, crow::json::type::Number, path, "number");
    return value.i();
}

CommandRegion parse_region(const crow::json::rvalue& region_json, const std::string& path) {
    require_json_type(region_json, crow::json::type::Object, path, "object");

    auto region = CommandRegion{
        require_int(region_json["x1"], path + ".x1"),
        require_int(region_json["y1"], path + ".y1"),
        require_int(region_json["x2"], path + ".x2"),
        require_int(region_json["y2"], path + ".y2"),
        require_string(region_json["command"], path + ".command"),
        require_string(region_json["label"], path + ".label")
    };

    if (region.x1 > region.x2) {
        throw_validation_error(path, "x1 must be less than or equal to x2");
    }

    if (region.y1 > region.y2) {
        throw_validation_error(path, "y1 must be less than or equal to y2");
    }

    return region;
}

Page parse_page(const crow::json::rvalue& page_json, const std::string& path) {
    require_json_type(page_json, crow::json::type::Object, path, "object");
    require_json_type(page_json["regions"], crow::json::type::List, path + ".regions", "array");

    auto page = Page{require_string(page_json["name"], path + ".name"), {}};
    for (std::size_t index = 0; index < page_json["regions"].size(); ++index) {
        page.regions.push_back(parse_region(page_json["regions"][index], path + ".regions[" + std::to_string(index) + "]"));
    }
    return page;
}

std::vector<Page> parse_pages_config(const crow::json::rvalue& config_json, const std::string& source_path) {
    require_json_type(config_json, crow::json::type::Object, source_path, "object");
    auto const& pages_json = config_json["pages"];
    require_json_type(pages_json, crow::json::type::List, source_path + ".pages", "array");

    if (pages_json.size() == 0) {
        throw_validation_error(source_path + ".pages", "must contain at least one page");
    }

    auto loaded_pages = std::vector<Page>{};
    loaded_pages.reserve(pages_json.size());
    for (std::size_t index = 0; index < pages_json.size(); ++index) {
        loaded_pages.push_back(parse_page(pages_json[index], source_path + ".pages[" + std::to_string(index) + "]"));
    }
    return loaded_pages;
}

std::string read_text_file(const std::string& path) {
    auto input = std::ifstream{path};
    if (!input) {
        throw std::runtime_error("failed to open pages file: " + path);
    }

    auto buffer = std::ostringstream{};
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("failed to read pages file: " + path);
    }

    return buffer.str();
}

std::vector<Page> load_pages_from_json_file(const std::string& path) {
    auto const file_contents = read_text_file(path);
    auto parsed_json = crow::json::load(file_contents);
    if (!parsed_json) {
        throw std::runtime_error("failed to parse JSON in pages file: " + path);
    }

    return parse_pages_config(parsed_json, path);
}

std::vector<Page> pages = make_default_pages();
int current_page_index = 0;

} // namespace

int main(int argc, char* argv[]) {
    try {
        auto const options = parse_command_line(argc, argv);
        if (options.should_exit) {
            return options.exit_code;
        }

        if (options.options.mPagesFile.has_value()) {
            pages = load_pages_from_json_file(*options.options.mPagesFile);
            current_page_index = 0;
            std::cout << "Loaded " << pages.size() << " pages from: " << *options.options.mPagesFile << "\n";
        } else {
            pages = make_default_pages();
            current_page_index = 0;
            std::cout << "Using built-in default pages configuration\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Startup error: " << ex.what() << "\n";
        return 1;
    }

    auto app = crow::SimpleApp{};

    // メインページ
    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::Get)
    ([]() {
        return crow::mustache::load_text("index.html");
    });

    // 現在のページ情報を取得
    CROW_ROUTE(app, "/page_info").methods(crow::HTTPMethod::Get)
    ([]() {
        auto res = crow::json::wvalue{};
        res["current_page"] = current_page_index;
        res["page_name"] = pages[current_page_index].name;
        res["total_pages"] = (int)pages.size();

        auto page_names = crow::json::wvalue::list{};
        for (const auto& p : pages) {
            page_names.push_back(p.name);
        }
        res["page_names"] = std::move(page_names);

        auto buttons = crow::json::wvalue::list{};
        for (const auto& region : pages[current_page_index].regions) {
            auto btn = crow::json::wvalue{};
            btn["label"] = region.label;
            btn["x1"] = region.x1;
            btn["y1"] = region.y1;
            btn["x2"] = region.x2;
            btn["y2"] = region.y2;
            buttons.push_back(std::move(btn));
        }
        res["buttons"] = std::move(buttons);
        return res;
    });

    // 特定のページに切り替え
    CROW_ROUTE(app, "/set_page/<int>").methods(crow::HTTPMethod::POST)
    ([](int page_idx) {
        if (page_idx < 0 || page_idx >= static_cast<int>(pages.size())) {
            return crow::response(400, "Invalid page index");
        }
        current_page_index = page_idx;
        std::cout << "Switched to page: " << current_page_index << " (" << pages[current_page_index].name << ")\n";
        return crow::response(200, std::to_string(current_page_index));
    });

    // クリックイベント
    CROW_ROUTE(app, "/click").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (not x) {
            return crow::response(400, "Invalid JSON");
        }

        auto const click_x = x["x"].i();
        auto const click_y = x["y"].i();

        auto executed_cmd = std::string{"None"};
        auto const& current_regions = pages[current_page_index].regions;

        for (auto const& region : current_regions) {
            if (click_x >= region.x1 && click_x <= region.x2 &&
                click_y >= region.y1 && click_y <= region.y2) {

                std::cout << "[" << pages[current_page_index].name << "] Executing: " << region.command << "\n";
                std::system(region.command.c_str());
                executed_cmd = region.command;
                break;
            }
        }

        auto res = crow::json::wvalue{};
        res["status"] = "success";
        res["command"] = executed_cmd;
        return crow::response(res);
    });

    app.port(8080).multithreaded().run();
}
