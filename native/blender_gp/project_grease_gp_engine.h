#pragma once

#include "project_grease_gp_backend.h"

namespace project_grease::gp {

/*
 * Bundled engine boundary for the Android host.
 *
 * This is deliberately an adapter over the real Blender 3.6.23 Legacy GP
 * backend. It is not a second drawing engine. New functionality should be
 * attached here only when its implementation is backed by the pinned Blender
 * Legacy GP source or by a minimal Android host adapter.
 */
enum class EngineFeature : unsigned char {
  Drawing,
  Pressure,
  Stabilizer,
  Eraser,
  Shapes,
  Fill,
  Layers,
  Frames,
  OnionSkin,
  Materials,
  Selection,
  Transform,
  Editing,
  Sculpt,
  VertexPaint,
  WeightPaint,
  Interpolation,
  Modifiers,
  LineArt,
  Rigging,
  SurfacePlacement,
  ThreeDDepth,
};

struct EngineFeatureState {
  EngineFeature feature;
  bool integrated;
  bool blender_backed;
};

/*
 * Returns the current bundled-engine capability map. The map is intentionally
 * descriptive: a feature must not be marked integrated until the actual
 * Blender 3.6.23 implementation is connected and exercised by a native test.
 */
const EngineFeatureState *engine_feature_map(int *count);

/*
 * Single host-facing entry point. The Backend remains the owner of the real
 * Legacy GP data; this facade prevents Android controllers from accumulating
 * Blender-specific details as the engine grows.
 */
class Engine {
 public:
  explicit Engine(Backend &backend) : backend_(backend) {}

  Backend &backend() { return backend_; }
  const Backend &backend() const { return backend_; }

 private:
  Backend &backend_;
};

}  // namespace project_grease::gp
