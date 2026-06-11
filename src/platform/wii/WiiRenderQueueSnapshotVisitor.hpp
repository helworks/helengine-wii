#pragma once

#include "IRenderVisitor3D.hpp"
#include "runtime/native_list.hpp"

namespace helengine::wii {
    /// Copies generated 3D render-queue items into a backend-local list without introducing GX knowledge.
    class WiiRenderQueueSnapshotVisitor : public IRenderVisitor3D {
    public:
        /// Creates an empty snapshot visitor before the queue walk begins.
        WiiRenderQueueSnapshotVisitor()
            : Items(new List<IDrawable3D*>()) {
        }

        /// Ordered queue contents captured from the generated camera queue.
        List<IDrawable3D*>* Items;

        /// Appends one queue item to the backend-local snapshot list.
        void Visit(IDrawable3D* drawable) override {
            Items->Add(drawable);
        }
    };
}
