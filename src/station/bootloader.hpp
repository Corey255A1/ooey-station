#pragma once

#include <string>
#include <vector>
#include <ooey/types.hpp>

namespace ooey_station::station {

struct GameInfo {
    std::string title;
    std::string author;
    std::string version;
    std::string description;
    std::string genre;
    std::string release_date;
    std::vector<ooey::Color> icon_colors;
    std::string palette_preview;
    std::string directory_path;
    std::string binary_path;
    bool is_valid{false};
};

class GameScanner {
public:
    GameScanner() = default;
    ~GameScanner() = default;

    static std::vector<GameInfo> scan_games(const std::string& games_dir);
};

class BootLoader {
public:
    enum class State {
        Splash,
        SystemCheck,
        Ready
    };

    BootLoader();
    ~BootLoader() = default;

    void update(float dt);
    State get_state() const { return state_; }
    float get_progress() const { return progress_; }

private:
    State state_{State::Splash};
    float timer_{0.0f};
    float progress_{0.0f};
};

} // namespace ooey_station::station
