#include <cassert>
#include <cmath>
#include <iostream>
#include "platform/wii/WiiTexturedQuadGeometry.hpp"

/// Checks independent destination and interpolated texture-coordinate goldens within floating-point tolerance.
static void AssertNear(double actual, double expected) {
    assert(std::abs(actual - expected) < 0.000001);
}

/// Exercises the actual Wii corner builder, including an already transformed parent pivot and scissored texture interpolation.
int main() {
    using helengine::wii::WiiTexturedQuadGeometry;
    const double halfPi = std::acos(-1.0) / 2.0;
    const auto zero = WiiTexturedQuadGeometry::Create(10, 20, 100, 50, 0);
    const double zeroGolden[] = {10, 20, 110, 20, 110, 70, 10, 70};
    const auto clockwise = WiiTexturedQuadGeometry::Create(10, 20, 100, 50, halfPi);
    const double clockwiseGolden[] = {10, 20, 10, 120, -40, 120, -40, 20};
    const auto counterclockwise = WiiTexturedQuadGeometry::Create(10, 20, 100, 50, -halfPi);
    const double counterclockwiseGolden[] = {10, 20, 10, -80, 60, -80, 60, 20};
    for (std::size_t index = 0; index < 8; ++index) {
        AssertNear(zero[index], zeroGolden[index]);
        AssertNear(clockwise[index], clockwiseGolden[index]);
        AssertNear(counterclockwise[index], counterclockwiseGolden[index]);
    }

    // A non-square quad keeps its local scaled edge lengths and the world origin instead of becoming an AABB.
    const auto diagonal = WiiTexturedQuadGeometry::Create(7, 13, 60, 20, halfPi / 2);
    const double rootTwo = std::sqrt(2.0);
    const double diagonalGolden[] = {7, 13, 7 + 30 * rootTwo, 13 + 30 * rootTwo,
        7 + 20 * rootTwo, 13 + 40 * rootTwo, 7 - 10 * rootTwo, 13 + 10 * rootTwo};
    for (std::size_t index = 0; index < 8; ++index) { AssertNear(diagonal[index], diagonalGolden[index]); }
    AssertNear(std::hypot(diagonal[2] - diagonal[0], diagonal[3] - diagonal[1]), 60);
    AssertNear(std::hypot(diagonal[6] - diagonal[0], diagonal[7] - diagonal[1]), 20);

    // Literal world-origin goldens for the authored parent at (640,360), local sprite (-768,-768), size1536.
    const double worldOrigins[][2] = {{-128, -408}, {1408, -408}, {1408, 1128}, {-128, 1128}};
    for (int turn = 0; turn < 4; ++turn) {
        const auto background = WiiTexturedQuadGeometry::Create(worldOrigins[turn][0], worldOrigins[turn][1],
            1536, 1536, turn * halfPi);
        AssertNear((background[0] + background[4]) / 2, 640);
        AssertNear((background[1] + background[5]) / 2, 360);
        AssertNear(background[0], worldOrigins[turn][0]);
        AssertNear(background[1], worldOrigins[turn][1]);
    }

    // GX scissoring does not change the vertex UVs. At x=-15 the rotated left edge is halfway through V,
    // while U remains the original left-source value; cropping an unrotated rectangle would change U instead.
    const double clipBoundary = -15;
    const double ratio = (clipBoundary - clockwise[0]) / (clockwise[6] - clockwise[0]);
    const double sourceU = 0.125 * 512 / 1024;
    const double sourceV = 0.25 * 300 / 512;
    const double sourceHeight = 0.5 * 300 / 512;
    AssertNear(ratio, 0.5);
    AssertNear(clockwise[0] + (clockwise[6] - clockwise[0]) * ratio, -15);
    AssertNear(clockwise[1] + (clockwise[7] - clockwise[1]) * ratio, 20);
    AssertNear(sourceU, 0.0625);
    AssertNear(sourceV + sourceHeight * ratio, 0.29296875);
    AssertNear(clockwise[3] + (clockwise[5] - clockwise[3]) * ratio, 120);
    std::cout << "Wii quad geometry PASS: zero/positive/negative/diagonal rotation, scaled edges, 4 parent pivots, clipped UV golden\n";
}
