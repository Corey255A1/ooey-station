#include "browser.hpp"
#include <iostream>

namespace ooey_station::station {

GameBrowser::GameBrowser() {
    set_width(gooey::SizePolicy::MatchParent);
    set_height(gooey::SizePolicy::MatchParent);
}

void GameBrowser::set_games(const std::vector<GameInfo>& games) {
    games_ = games;
    selected_index_ = 0;
    rebuild_ui();
}

void GameBrowser::rebuild_ui() {
    clear_children();
    // In this basic version, we do drawing directly in the draw() override 
    // to maintain a retro, low-res console theme.
}

void GameBrowser::draw(ooey::IRenderTarget& target) const {
    // Clear screen to deep blue retro background
    target.clear(ooey::Color(10, 15, 30));

    ooey::Font title_font("monospace", 24, ooey::FontWeight::Bold);
    ooey::Font normal_font("monospace", 14, ooey::FontWeight::Normal);

    // Draw Title Header
    target.draw_text("=== OOEY-STATION ===", title_font, ooey::Point(150, 40), ooey::Color(255, 255, 0));
    target.draw_text("SELECT A GAME TO PLAY", normal_font, ooey::Point(220, 80), ooey::Color(200, 200, 200));

    // Left Panel: Game list
    int y = 140;
    int index = 0;
    for (const auto& game : games_) {
        std::string prefix = (index == selected_index_) ? "> " : "  ";
        ooey::Color text_color = (index == selected_index_) ? ooey::Color(0, 255, 0) : ooey::Color(255, 255, 255);
        
        target.draw_text(prefix + game.title, normal_font, ooey::Point(50, y), text_color);
        y += 28;
        index++;
    }

    if (games_.empty()) {
        target.draw_text("No games found in games/ directory", normal_font, ooey::Point(50, 140), ooey::Color(255, 100, 100));
        target.draw_text("Place .booey binaries inside games/ to play!", normal_font, ooey::Point(50, 170), ooey::Color(150, 150, 150));
    }

    // Right Panel: Selected Game Detail
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(games_.size())) {
        const auto& game = games_[selected_index_];
        
        target.draw_text("TITLE: " + game.title, normal_font, ooey::Point(350, 140), ooey::Color(255, 255, 255));
        target.draw_text("AUTHOR: " + game.author, normal_font, ooey::Point(350, 170), ooey::Color(200, 200, 200));
        target.draw_text("VERSION: " + game.version, normal_font, ooey::Point(350, 200), ooey::Color(200, 200, 200));
        
        target.draw_text("DESCRIPTION:", normal_font, ooey::Point(350, 240), ooey::Color(255, 255, 0));
        
        // Wrap description text roughly
        int desc_y = 270;
        size_t pos = 0;
        std::string desc = game.description;
        while (pos < desc.length()) {
            std::string line = desc.substr(pos, 35);
            target.draw_text(line, normal_font, ooey::Point(350, desc_y), ooey::Color(220, 220, 220));
            pos += 35;
            desc_y += 20;
        }

        target.draw_text("[ PRESS START / ENTER TO PLAY ]", normal_font, ooey::Point(350, 420), ooey::Color(0, 255, 0));
    }

    // Settings prompt
    target.draw_text("[UP/DOWN] Navigate   [ENTER] Select", normal_font, ooey::Point(150, 450), ooey::Color(120, 120, 120));
}

void GameBrowser::handle_input(ooey::InputManager* input) {
    if (!input || games_.empty()) return;

    for (const auto& event : input->get_key_events()) {
        if (event.state == ooey::KeyState::Pressed) {
            int k = event.key_code;
            if (k == 65362 || k == 119 || k == 87 || k == 38 || k == 103) { // Up, 'w', 'W', VK_UP, KEY_UP
                selected_index_ = std::max(0, selected_index_ - 1);
            } else if (k == 65364 || k == 115 || k == 83 || k == 40 || k == 108) { // Down, 's', 'S', VK_DOWN, KEY_DOWN
                selected_index_ = std::min(static_cast<int>(games_.size()) - 1, selected_index_ + 1);
            } else if (k == 65293 || k == 13 || k == 10 || k == 36 || k == 28) { // Enter, Return, VK_RETURN, KEY_ENTER
                if (on_launch_game_) {
                    on_launch_game_(games_[selected_index_]);
                }
            }
        }
    }
}

} // namespace ooey_station::station
