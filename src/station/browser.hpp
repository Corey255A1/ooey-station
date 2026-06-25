#pragma once

#include "bootloader.hpp"
#include <gooey/mvvmc/gooey_node.hpp>
#include <ooey/input.hpp>
#include <functional>
#include <vector>

namespace ooey_station::station {

class GameBrowser : public gooey::GooeyNode {
public:
    GameBrowser();
    ~GameBrowser() override = default;

    void set_games(const std::vector<GameInfo>& games);
    void set_on_launch_game(std::function<void(const GameInfo&)> cb) { on_launch_game_ = cb; }

    void handle_input(ooey::InputManager* input);

    // Override draw to customize the retro theme
    void draw(ooey::IRenderTarget& target) const override;

private:
    std::vector<GameInfo> games_;
    int selected_index_{0};
    std::function<void(const GameInfo&)> on_launch_game_;

    void rebuild_ui();
};

} // namespace ooey_station::station
