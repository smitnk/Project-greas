#include "project_grease_gp_engine.h"

namespace project_grease::gp {

static constexpr EngineFeatureState kEngineFeatures[] = {
    {EngineFeature::Drawing, true, true},
    {EngineFeature::Pressure, true, true},
    {EngineFeature::Stabilizer, true, true},
    {EngineFeature::Eraser, true, true},
    {EngineFeature::Shapes, true, true},
    {EngineFeature::Fill, true, true},
    {EngineFeature::Layers, true, true},
    {EngineFeature::Frames, true, true},
    {EngineFeature::OnionSkin, true, true},
    {EngineFeature::Materials, true, true},
    {EngineFeature::Selection, true, true},
    {EngineFeature::Transform, true, true},
    {EngineFeature::Editing, true, true},
    {EngineFeature::Sculpt, false, true},
    {EngineFeature::VertexPaint, false, true},
    {EngineFeature::WeightPaint, false, true},
    // Phase 2 interpolation adapter is intentionally not advertised as complete:\n    // the current backend still requires matched stroke topology.\n    {EngineFeature::Interpolation, false, true},
    {EngineFeature::Modifiers, false, true},
    {EngineFeature::LineArt, false, true},
    {EngineFeature::Rigging, false, true},
    {EngineFeature::SurfacePlacement, false, true},
    {EngineFeature::ThreeDDepth, false, true},
};

const EngineFeatureState *engine_feature_map(int *count)
{
  if (count) {
    *count = static_cast<int>(sizeof(kEngineFeatures) / sizeof(kEngineFeatures[0]));
  }
  return kEngineFeatures;
}

}  // namespace project_grease::gp
