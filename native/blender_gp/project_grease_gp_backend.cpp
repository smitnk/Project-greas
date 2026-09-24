#include "project_grease_gp_backend.h"

#include <string>

namespace project_grease::gp {

struct Backend::Impl {
  std::string last_error;
  bool initialized = false;
  bool document_created = false;
  bool layer_created = false;
  bool frame_created = false;
  bool stroke_open = false;
};

Backend::Backend() : impl_(new Impl()) {}

Backend::~Backend() {
  shutdown();
  delete impl_;
}

bool Backend::initialize() {
  // Runtime initialization is deliberately left behind the Blender-native
  // dependency gate. This function must not fall back to Android Canvas.
  impl_->initialized = true;
  return true;
}

void Backend::shutdown() {
  if (!impl_) {
    return;
  }
  impl_->stroke_open = false;
  impl_->frame_created = false;
  impl_->layer_created = false;
  impl_->document_created = false;
  impl_->initialized = false;
}

bool Backend::create_document() {
  if (!impl_->initialized) {
    impl_->last_error = "backend is not initialized";
    return false;
  }
  impl_->document_created = true;
  return true;
}

bool Backend::create_layer(const char* /*name*/) {
  if (!impl_->document_created) {
    impl_->last_error = "document is not created";
    return false;
  }
  impl_->layer_created = true;
  return true;
}

bool Backend::create_frame(int /*frame_number*/) {
  if (!impl_->layer_created) {
    impl_->last_error = "layer is not created";
    return false;
  }
  impl_->frame_created = true;
  return true;
}

bool Backend::begin_stroke(const StrokeStyle& /*style*/) {
  if (!impl_->frame_created) {
    impl_->last_error = "frame is not created";
    return false;
  }
  impl_->stroke_open = true;
  return true;
}

bool Backend::add_point(const StrokePoint& /*point*/) {
  if (!impl_->stroke_open) {
    impl_->last_error = "stroke is not open";
    return false;
  }
  return true;
}

bool Backend::end_stroke() {
  if (!impl_->stroke_open) {
    impl_->last_error = "stroke is not open";
    return false;
  }
  impl_->stroke_open = false;
  return true;
}

bool Backend::render() {
  if (!impl_->frame_created) {
    impl_->last_error = "nothing to render";
    return false;
  }
  impl_->last_error =
      "GP draw-cache/DRW render target is not linked yet";
  return false;
}

const char* Backend::last_error() const {
  return impl_->last_error.c_str();
}

}  // namespace project_grease::gp
