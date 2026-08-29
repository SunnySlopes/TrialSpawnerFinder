#include "core/Types.hpp"

#include <stdexcept>

namespace trialfinder {

const BlockPos BlockPos::ZERO = BlockPos(0, 0, 0);

std::string BoundingBox::toString() const {
    return "BoundingBox[" + std::to_string(minX) + ", " + std::to_string(minY)
         + ", " + std::to_string(minZ) + " -> " + std::to_string(maxX)
         + ", " + std::to_string(maxY) + ", " + std::to_string(maxZ) + "]";
}

const char* directionSerializedName(Direction d) {
    switch (d) {
        case Direction::DOWN: return "down";
        case Direction::UP: return "up";
        case Direction::NORTH: return "north";
        case Direction::SOUTH: return "south";
        case Direction::WEST: return "west";
        case Direction::EAST: return "east";
    }
    return "down";
}

Rotation rotationByIndex(int index) {
    switch (index) {
        case 1: return Rotation::CLOCKWISE_90;
        case 2: return Rotation::CLOCKWISE_180;
        case 3: return Rotation::COUNTERCLOCKWISE_90;
        default: return Rotation::NONE;
    }
}

// Java FrontAndTop 的 12 个合法组合（serializedName -> (front, top)）。
struct FrontTopEntry { const char* name; Direction front; Direction top; };
static const FrontTopEntry FRONT_TOP_TABLE[] = {
    {"down_east",  Direction::DOWN,  Direction::EAST},
    {"down_north", Direction::DOWN,  Direction::NORTH},
    {"down_south", Direction::DOWN,  Direction::SOUTH},
    {"down_west",  Direction::DOWN,  Direction::WEST},
    {"up_east",    Direction::UP,    Direction::EAST},
    {"up_north",   Direction::UP,    Direction::NORTH},
    {"up_south",   Direction::UP,    Direction::SOUTH},
    {"up_west",    Direction::UP,    Direction::WEST},
    {"east_up",    Direction::EAST,  Direction::UP},
    {"north_up",   Direction::NORTH, Direction::UP},
    {"south_up",   Direction::SOUTH, Direction::UP},
    {"west_up",    Direction::WEST,  Direction::UP},
};

FrontAndTop FrontAndTop::parse(const std::string& serializedName) {
    for (const FrontTopEntry& entry : FRONT_TOP_TABLE) {
        if (serializedName == entry.name) {
            return FrontAndTop{entry.front, entry.top};
        }
    }
    throw std::invalid_argument("未知 FrontAndTop: " + serializedName);
}

FrontAndTop FrontAndTop::fromFrontAndTop(Direction front, Direction top) {
    for (const FrontTopEntry& entry : FRONT_TOP_TABLE) {
        if (entry.front == front && entry.top == top) {
            return FrontAndTop{front, top};
        }
    }
    throw std::invalid_argument("无效的 front/top 组合");
}

} // namespace trialfinder
