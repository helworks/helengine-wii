#pragma once

#include <array>
#include <cmath>

namespace helengine::wii {
    /// Builds GX sprite corners from the shared command's already transformed world origin and scaled local axes.
    class WiiTexturedQuadGeometry {
    public:
        /// Returns top-left, top-right, bottom-right and bottom-left XY pairs without replacing the rotated quad with its bounding box.
        /// The origin has already been transformed by its parent; rotating around the rectangle center again would move that pivot.
        /// UVs stay attached to these four corners and GX scissors the transformed primitive, preserving texture interpolation.
        static std::array<double, 8> Create(double x, double y, double width, double height, double rotationRadians) {
            const double cosine = std::cos(rotationRadians);
            const double sine = std::sin(rotationRadians);
            const double rightX = width * cosine;
            const double rightY = width * sine;
            const double downX = -height * sine;
            const double downY = height * cosine;
            return {x, y, x + rightX, y + rightY,
                x + rightX + downX, y + rightY + downY, x + downX, y + downY};
        }
    };
}
