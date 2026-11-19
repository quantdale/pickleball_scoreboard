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
        struct Frame {
            std::string rows[64];
            int error = 0;
        } frame;
        for (auto& row : frame.rows) row.assign(16, '0');
        renderPortraitPixels(state, [](void* context, int x, int y) {
            auto& frame = *static_cast<Frame*>(context);
            if (x < 0 || x >= 16 || y < 0 || y >= 64) { frame.error = 3; return; }
            if (frame.rows[y][x] != '0') frame.error = 4;
            frame.rows[y][x] = '1';
        }, &frame);
        if (frame.error) return frame.error;
        std::cout << parts[0] << '\n';
        for (const auto& row : frame.rows) std::cout << row << '\n';
    }
    return 0;
}
