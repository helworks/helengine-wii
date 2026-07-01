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

        List<RenderFrame*>* frames = extractionResult->get_Frames();
        if (frames != nullptr) {
            for (int32_t frameIndex = 0; frameIndex < frames->get_Count(); frameIndex++) {
                RenderFrame* frame = (*frames)[frameIndex];
                if (frame == nullptr) {
                    continue;
                }

                const bool deleteSharedSubmissionItems = frameIndex == 0;
                List<RenderFrameDrawableSubmission*>* drawableSubmissions = frame->get_DrawableSubmissions();
                if (drawableSubmissions != nullptr) {
                    if (deleteSharedSubmissionItems) {
                        for (int32_t submissionIndex = 0; submissionIndex < drawableSubmissions->get_Count(); submissionIndex++) {
                            RenderFrameDrawableSubmission* drawableSubmission = (*drawableSubmissions)[submissionIndex];
                            if (drawableSubmission == nullptr) {
                                continue;
                            }

                            RenderFrameBatchingMetadata* batchingMetadata = drawableSubmission->get_BatchingMetadata();
                            if (batchingMetadata != nullptr) {
                                delete batchingMetadata;
                            }

                            delete drawableSubmission;
                        }
                    }

                    delete drawableSubmissions;
                }

                List<RenderFrameLightSubmission*>* lightSubmissions = frame->get_LightSubmissions();
                if (lightSubmissions != nullptr) {
                    if (deleteSharedSubmissionItems) {
                        for (int32_t submissionIndex = 0; submissionIndex < lightSubmissions->get_Count(); submissionIndex++) {
                            RenderFrameLightSubmission* lightSubmission = (*lightSubmissions)[submissionIndex];
                            if (lightSubmission == nullptr) {
                                continue;
                            }

                            delete lightSubmission;
                        }
                    }

                    delete lightSubmissions;
                }

                List<RenderFrameShadowCasterSubmission*>* shadowCasterSubmissions = frame->get_ShadowCasterSubmissions();
                if (shadowCasterSubmissions != nullptr) {
                    if (deleteSharedSubmissionItems) {
                        for (int32_t submissionIndex = 0; submissionIndex < shadowCasterSubmissions->get_Count(); submissionIndex++) {
                            RenderFrameShadowCasterSubmission* shadowCasterSubmission = (*shadowCasterSubmissions)[submissionIndex];
                            if (shadowCasterSubmission == nullptr) {
                                continue;
                            }

                            delete shadowCasterSubmission;
                        }
                    }

                    delete shadowCasterSubmissions;
                }

                delete frame;
            }

            delete frames;
        }

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
