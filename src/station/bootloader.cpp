#include "bootloader.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

namespace ooey_station::station {

std::vector<GameInfo> GameScanner::scan_games(const std::string& games_dir) {
    std::vector<GameInfo> games;
    
    if (!fs::exists(games_dir) || !fs::is_directory(games_dir)) {
        std::cerr << "Scanner Error: Games directory does not exist: " << games_dir << std::endl;
        return games;
    }

    for (const auto& entry : fs::directory_iterator(games_dir)) {
        if (entry.is_directory()) {
            GameInfo info;
            info.directory_path = entry.path().string();
            info.title = entry.path().filename().string(); // Default to folder name
            info.description = "No description provided.";
            
            // Check for game.txt metadata
            fs::path meta_path = entry.path() / "game.txt";
            if (fs::exists(meta_path)) {
                std::ifstream in(meta_path);
                std::string line;
                while (std::getline(in, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    
                    auto eq = line.find('=');
                    if (eq != std::string::npos) {
                        std::string key = line.substr(0, eq);
                        std::string val = line.substr(eq + 1);
                        
                        if (key == "title") info.title = val;
                        else if (key == "author") info.author = val;
                        else if (key == "version") info.version = val;
                        else if (key == "description") info.description = val;
                        else if (key == "genre") info.genre = val;
                        else if (key == "release_date") info.release_date = val;
                    }
                }
            }

            // Check for binary
            fs::path bin_path = entry.path() / "game.booey.bin";
            if (!fs::exists(bin_path)) {
                bin_path = entry.path() / "game.booey";
            }
            
            if (fs::exists(bin_path)) {
                info.binary_path = bin_path.string();
                info.is_valid = true;
                games.push_back(info);
            }
        }
    }

    return games;
}

BootLoader::BootLoader() {}

void BootLoader::update(float dt) {
    timer_ += dt;
    
    switch (state_) {
        case State::Splash:
            progress_ = std::min(timer_ / 1.5f, 1.0f);
            if (timer_ >= 1.5f) {
                state_ = State::SystemCheck;
                timer_ = 0.0f;
            }
            break;
            
        case State::SystemCheck:
            progress_ = std::min(timer_ / 0.8f, 1.0f);
            if (timer_ >= 0.8f) {
                state_ = State::Ready;
                timer_ = 0.0f;
            }
            break;
            
        case State::Ready:
            progress_ = 1.0f;
            break;
    }
}

} // namespace ooey_station::station
