#pragma once
#include <cstdint>
#include <cstddef>
// Minimal ABI declarations adapted from sd88me/mpc-vst-plugins wrapper/vst2_wrap.c.
// Copyright (c) 2026 sd88me. MIT notice: research/mpc-vst-plugins-LICENSE.txt.
// No Steinberg SDK headers are included or bundled.
namespace mpccv::vst {
struct Effect;
using Callback=intptr_t(*)(Effect*,int32_t,int32_t,intptr_t,void*,float);
using Process=void(*)(Effect*,float**,float**,int32_t);
struct Effect {
    int32_t magic;
    Callback dispatcher;
    Process process;
    void (*setParameter)(Effect*,int32_t,float);
    float (*getParameter)(Effect*,int32_t);
    int32_t numPrograms,numParams,numInputs,numOutputs,flags;
    intptr_t reserved1,reserved2;
    int32_t initialDelay,realQualities,offQualities;
    float ioRatio;
    void *object,*user;
    int32_t uniqueID,version;
    Process processReplacing;
    void (*processDoubleReplacing)(Effect*,double**,double**,int32_t);
    char future[56];
};
struct Time {
    double samplePos,sampleRate,nanoSeconds,ppqPos,tempo,barStartPos,cycleStartPos,cycleEndPos;
    int32_t timeSigNumerator,timeSigDenominator,smpteOffset,smpteFrameRate,samplesToNextClock,flags;
};
constexpr int32_t magic=0x56737450, identity=0x4d634376; // VstP, McCv
constexpr unsigned parameter_count=26;
static_assert(sizeof(Time)==88,"VST time ABI mismatch");
static_assert(sizeof(Effect)==(sizeof(void*)==8?192:144),"VST effect ABI mismatch");
struct Telemetry { uint32_t codes[8],playing,armed,fault,gate_rises[8]; };
}
extern "C" mpccv::vst::Effect* VSTPluginMain(mpccv::vst::Callback);
extern "C" int mpc_cv_read_telemetry(mpccv::vst::Effect*,mpccv::vst::Telemetry*);
