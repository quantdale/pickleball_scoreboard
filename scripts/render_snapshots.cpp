// Host-only exporter of the ACTUAL firmware renderer for cross-language fixtures.
#include "display_render_logic.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line.find("name,") == 0) continue;
        std::stringstream stream(line);
        std::vector<std::string> parts;
        std::string part;
        while (std::getline(stream, part, ',')) parts.push_back(part);
        if (parts.size() != 7) return 2;
        GameState state;
        state.leftScore = std::stoi(parts[1]); state.rightScore = std::stoi(parts[2]);
        state.servingSide = parts[3] == "L" ? Side::LEFT : Side::RIGHT;
        state.serverNumber = std::stoi(parts[4]); state.gameEnded = parts[5] == "1";
        state.gameMode = parts[6][0] == 'S' ? GameMode::SINGLES : GameMode::DOUBLES;
        std::string rows[16];
        for (auto& row : rows) row.assign(64, '0');
        for (const auto& pixel : computeRenderedPixels(state)) {
            if (pixel.x < 0 || pixel.x >= 64 || pixel.y < 0 || pixel.y >= 16) return 3;
            if (rows[pixel.y][pixel.x] != '0') return 4;
            rows[pixel.y][pixel.x] = '1';
        }
        std::cout << parts[0] << '\n';
        for (const auto& row : rows) std::cout << row << '\n';
    }
    return 0;
}
