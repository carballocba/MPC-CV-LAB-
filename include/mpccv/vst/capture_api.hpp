#pragma once
#include "abi.hpp"
#include "mpccv/capture_types.hpp"
// Optional diagnostic control API. Never call these mutex/worker lifecycle
// operations from audio, and never treat capture acknowledgement as voltage.
extern "C" MPCCV_EXPORT int32_t mpc_cv_capture_start(mpccv::vst::Effect*,uint32_t reserved_mask);
extern "C" MPCCV_EXPORT int32_t mpc_cv_capture_stop(mpccv::vst::Effect*,mpccv::CaptureStatus* last);
extern "C" MPCCV_EXPORT int32_t mpc_cv_capture_status(mpccv::vst::Effect*,mpccv::CaptureStatus*);
extern "C" MPCCV_EXPORT uint32_t mpc_cv_capture_copy(mpccv::vst::Effect*,mpccv::CaptureRecord*,uint32_t capacity);
