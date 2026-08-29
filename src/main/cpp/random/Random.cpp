#include "random/Random.hpp"

#include "core/Util.hpp"

namespace trialfinder {

Rotation rotationGetRandom(RandomSource& random) {
    Rotation rotations[] = {
        Rotation::NONE, Rotation::CLOCKWISE_90,
        Rotation::CLOCKWISE_180, Rotation::COUNTERCLOCKWISE_90
    };
    return rotations[random.nextInt(4)];
}

std::vector<Rotation> rotationGetShuffled(RandomSource& random) {
    std::vector<Rotation> list = {
        Rotation::NONE, Rotation::CLOCKWISE_90,
        Rotation::CLOCKWISE_180, Rotation::COUNTERCLOCKWISE_90
    };
    Util::shuffle(list, random);
    return list;
}

} // namespace trialfinder
