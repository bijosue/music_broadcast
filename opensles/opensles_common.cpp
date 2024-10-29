#include "opensles_common.h"
#include <stdio.h>

#include <SLES/OpenSLES.h>

#include "arraysize.h"

const char* GetSLErrorString(size_t code) {
    static const char* sl_error_strings[] = {
            "SL_RESULT_SUCCESS",                 // 0
            "SL_RESULT_PRECONDITIONS_VIOLATED",  // 1
            "SL_RESULT_PARAMETER_INVALID",       // 2
            "SL_RESULT_MEMORY_FAILURE",          // 3
            "SL_RESULT_RESOURCE_ERROR",          // 4
            "SL_RESULT_RESOURCE_LOST",           // 5
            "SL_RESULT_IO_ERROR",                // 6
            "SL_RESULT_BUFFER_INSUFFICIENT",     // 7
            "SL_RESULT_CONTENT_CORRUPTED",       // 8
            "SL_RESULT_CONTENT_UNSUPPORTED",     // 9
            "SL_RESULT_CONTENT_NOT_FOUND",       // 10
            "SL_RESULT_PERMISSION_DENIED",       // 11
            "SL_RESULT_FEATURE_UNSUPPORTED",     // 12
            "SL_RESULT_INTERNAL_ERROR",          // 13
            "SL_RESULT_UNKNOWN_ERROR",           // 14
            "SL_RESULT_OPERATION_ABORTED",       // 15
            "SL_RESULT_CONTROL_LOST",            // 16
    };

    if (code >= arraysize(sl_error_strings)) {
        return "SL_RESULT_UNKNOWN_ERROR";
    }
    return sl_error_strings[code];
}

SLDataFormat_PCM CreatePCMConfiguration(int sample_rate, size_t channels, size_t bits_per_sample)
{
    SLDataFormat_PCM format;
    format.formatType = SL_DATAFORMAT_PCM;
    format.numChannels = static_cast<SLuint32>(channels);
    switch (sample_rate) {
        case 8000:
            format.samplesPerSec = SL_SAMPLINGRATE_8;
            break;
        case 16000:
            format.samplesPerSec = SL_SAMPLINGRATE_16;
            break;
        case 22050:
            format.samplesPerSec = SL_SAMPLINGRATE_22_05;
            break;
        case 32000:
            format.samplesPerSec = SL_SAMPLINGRATE_32;
            break;
        case 44100:
            format.samplesPerSec = SL_SAMPLINGRATE_44_1;
            break;
        case 48000:
            format.samplesPerSec = SL_SAMPLINGRATE_48;
            break;
        case 64000:
            format.samplesPerSec = SL_SAMPLINGRATE_64;
            break;
        case 88200:
            format.samplesPerSec = SL_SAMPLINGRATE_88_2;
            break;
        case 96000:
            format.samplesPerSec = SL_SAMPLINGRATE_96;
            break;
        default:
            break;
    }
    format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    format.endianness = SL_BYTEORDER_LITTLEENDIAN;
    if (format.numChannels == 1) {
        format.channelMask = SL_SPEAKER_FRONT_CENTER;
    } else if (format.numChannels == 2) {
        format.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }
    return format;
}

std::unique_ptr<OpenSLEngineManager> engine_manager = std::make_unique<OpenSLEngineManager>();

OpenSLEngineManager::OpenSLEngineManager()
{

}

SLObjectItf OpenSLEngineManager::GetOpenSLEngine()
{
    if (engine_object_.Get() != nullptr) {
              return engine_object_.Get();
    }
    // Create the engine object in thread safe mode.
    const SLEngineOption option[] = {
            {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
    SLresult result =
            slCreateEngine(engine_object_.Receive(), 1, option, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        fprintf(stderr, "slCreateEngine() failed: %s\n", GetSLErrorString(result));
        engine_object_.Reset();
        return nullptr;
    }
    // Realize the SL Engine in synchronous mode.
    result = engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {

        engine_object_.Reset();
        return nullptr;
    }
    // Finally return the SLObjectItf interface of the engine object.
    return engine_object_.Get();
}
