#include "platform/wii/WiiFramePlan.hpp"

#include "IDrawable3D.hpp"
#include "LightComponent.hpp"
#include "RenderFrame.hpp"
#include "RenderFrameBatchingMetadata.hpp"
#include "RenderFrameShadowCasterSubmission.hpp"

namespace {
    /// Releases the extracted render-frame graph owned by one Wii frame plan.
    void DeleteExtractionResult(RenderFrameExtractionResult*& extractionResult) {
        if (extractionResult == nullptr) {
            return;
        }

        IReadOnlyList<RenderFrame*>* frames = extractionResult->get_Frames();
        if (frames != nullptr) {
            for (int32_t frameIndex = 0; frameIndex < frames->get_Count(); frameIndex++) {
                RenderFrame* frame = frames->get_Item(frameIndex);
                if (frame == nullptr) {
                    continue;
                }

                const bool deleteSharedSubmissionItems = frameIndex == 0;
                IReadOnlyList<RenderFrameDrawableSubmission*>* drawableSubmissions = frame->get_DrawableSubmissions();
                if (drawableSubmissions != nullptr) {
                    if (deleteSharedSubmissionItems) {
                        for (int32_t submissionIndex = 0; submissionIndex < drawableSubmissions->get_Count(); submissionIndex++) {
                            RenderFrameDrawableSubmission* drawableSubmission = drawableSubmissions->get_Item(submissionIndex);
                            if (drawableSubmission == nullptr) {
                                continue;
                            }

                            RenderFrameBatchingMetadata* batchingMetadata = drawableSubmission->get_BatchingMetadata();
                            if (batchingMetadata != nullptr) {
                                delete batchingMetadata;
                                drawableSubmission->BatchingMetadata = nullptr;
                            }
                        }
                    }
                }
            }
        }

        extractionResult->Dispose();
        delete extractionResult;
        extractionResult = nullptr;
    }
}

namespace helengine::wii {
    /// Releases the temporary scene snapshots used to build this frame plan.
    WiiFramePlan::~WiiFramePlan() {
        if (Cameras != nullptr) {
            delete Cameras;
            Cameras = nullptr;
        }

        if (Drawables != nullptr) {
            delete Drawables;
            Drawables = nullptr;
        }

        if (Lights != nullptr) {
            delete Lights;
            Lights = nullptr;
        }

        DeleteExtractionResult(ExtractionResult);
        DrawableSubmissions = nullptr;
        LightSubmissions = nullptr;
    }
}
