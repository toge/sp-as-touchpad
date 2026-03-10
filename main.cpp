#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

#include "crow.h"

struct CommandRegion {
    int x1, y1, x2, y2;
    std::string command;
    std::string label;
};

struct Page {
    std::string name;
    std::vector<CommandRegion> regions;
};

// 複数ページの設定
auto pages = std::vector<Page>{
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

auto current_page_index = 0;

int main() {
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
