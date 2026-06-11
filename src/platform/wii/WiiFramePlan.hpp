#pragma once

#include "CameraComponent.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RenderFrameLightSubmission.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "runtime/native_list.hpp"

namespace helengine::wii {
    /// Owns the reduced execution payload for one Wii camera frame.
    class WiiFramePlan {
    public:
        /// Creates one frame plan with resolved camera state, extracted submissions, and both logical and physical viewports.
        WiiFramePlan(
            CameraComponent* camera,
            List<RenderFrameDrawableSubmission*>* drawableSubmissions,
            List<RenderFrameLightSubmission*>* lightSubmissions,
            float4 logicalViewport,
            float4 physicalViewport,
            float4x4 view,
            float4x4 projection,
            float4x4 viewProjection)
            : Camera(camera)
            , DrawableSubmissions(drawableSubmissions)
            , LightSubmissions(lightSubmissions)
            , LogicalViewport(logicalViewport)
            , PhysicalViewport(physicalViewport)
            , View(view)
            , Projection(projection)
            , ViewProjection(viewProjection) {
        }

        /// Camera that owns the render queues and authored clear settings for this frame.
        CameraComponent* Camera;

        /// Opaque drawable submissions extracted from the generated runtime graph.
        List<RenderFrameDrawableSubmission*>* DrawableSubmissions;

        /// Light submissions extracted from the generated runtime graph for this frame.
        List<RenderFrameLightSubmission*>* LightSubmissions;

        /// Logical viewport resolved from the authored runtime viewport in shared-engine window space.
        float4 LogicalViewport;

        /// Physical viewport resolved from the logical viewport for GX viewport and scissor setup.
        float4 PhysicalViewport;

        /// View matrix derived from the authored runtime camera transform.
        float4x4 View;

        /// Perspective projection derived from the authored runtime camera settings.
        float4x4 Projection;

        /// Cached view-projection matrix reused by the raster renderer.
        float4x4 ViewProjection;
    };
}
