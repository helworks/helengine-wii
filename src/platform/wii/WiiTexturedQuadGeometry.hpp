#pragma once

#include <array>
#include <cmath>
#if HELENGINE_WII_HAS_GENERATED_CORE
#include "RealMath.hpp"
#endif

namespace helengine::wii {
    /// Builds GX sprite corners from the shared command's already transformed world origin and scaled local axes.
    class WiiTexturedQuadGeometry {
    public:
        /// Returns top-left, top-right, bottom-right and bottom-left XY pairs without replacing the rotated quad with its bounding box.
        /// The origin has already been transformed by its parent; rotating around the rectangle center again would move that pivot.
        /// UVs stay attached to these four corners and GX scissors the transformed primitive, preserving texture interpolation.
#if HELENGINE_WII_HAS_GENERATED_CORE
        /// Keeps generated engine coordinates and trigonometry at the configured arithmetic width until GX submission.
        static std::array<RealEvaluation, 8> CreateManaged(RealEvaluation x, RealEvaluation y, RealEvaluation width, RealEvaluation height, RealEvaluation rotationRadians) {
            const RealEvaluation cosine = RealMath::Cos(rotationRadians);
            const RealEvaluation sine = RealMath::Sin(rotationRadians);
            const RealEvaluation rightX = width * cosine;
            const RealEvaluation rightY = width * sine;
            const RealEvaluation downX = -height * sine;
            const RealEvaluation downY = height * cosine;
            return {x, y, x + rightX, y + rightY,
                x + rightX + downX, y + rightY + downY, x + downX, y + downY};
        }
#endif
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
