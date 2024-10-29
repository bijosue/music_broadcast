#include <android/log.h>

#include "opensles_recorder.h"
#include "audio_manager.h"

#include "arraysize.h"

#define TAG "OpenSLESRecorder"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#define LOG_ON_ERROR(op)                                    \
  [](SLresult err) {                                        \
    if (err != SL_RESULT_SUCCESS) {                         \
      ALOGE("%s:%d %s failed: %s", __FILE__, __LINE__, #op, \
            GetSLErrorString(err));                         \
      return true;                                          \
    }                                                       \
    return false;                                           \
  }(op)

OpenSLESRecorder::OpenSLESRecorder()
{
    initialized_ = false;
    recording_ = false;
    engine_ = nullptr;
    recorder_ = nullptr;
    simple_buffer_queue_ = nullptr;
    buffer_index_ = 0;

}

OpenSLESRecorder::~OpenSLESRecorder()
{
    this->Close();
    engine_ = nullptr;
}

int OpenSLESRecorder::Open(int sample_rate, int channels)
{
    if (initialized_ || recording_)
        return -1;

    cap_buffer.reset();

    pcm_format_ = CreatePCMConfiguration(sample_rate, channels, SL_PCMSAMPLEFORMAT_FIXED_16);
    AllocateDataBuffers(sample_rate, channels);

    InitRecording();
    StartRecording();
    return recording_ ? 0 : -1;
}

int OpenSLESRecorder::Close()
{
    StopRecording();
    DestroyAudioRecorder();
    engine_ = nullptr;
    return 0;
}

int OpenSLESRecorder::Read(uint8_t *data, int length)
{
    return cap_buffer.read(data, length);
}

int OpenSLESRecorder::InitRecording()
{
    ALOGD("InitRecording");
    if (initialized_ || recording_)
        return -1;

    if (!ObtainEngineInterface()) {
        ALOGE("Failed to obtain SL Engine interface");
        return -1;
    }
    CreateAudioRecorder();
    initialized_ = true;
    buffer_index_ = 0;
    return 0;
}

int OpenSLESRecorder::StartRecording()
{
    ALOGD("StartRecording");
    if (!initialized_ || recording_)
        return -1;

    int num_buffers_in_queue = GetBufferCount();
    for (int i = 0; i < kNumOfOpenSLESBuffers - num_buffers_in_queue; ++i) {
        if (!EnqueueAudioBuffer()) {
            recording_ = false;
            return -1;
        }
    }
    num_buffers_in_queue = GetBufferCount();
    if (num_buffers_in_queue != kNumOfOpenSLESBuffers)
        return -1;
    LogBufferState();

    if (LOG_ON_ERROR((*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING))) {
        return -1;
    }
    recording_ = (GetRecordState() == SL_RECORDSTATE_RECORDING);
    return recording_ ? 0 : -1;
}

int OpenSLESRecorder::StopRecording()
{
    ALOGD("StopRecording");
    if (!initialized_ || !recording_) {
        return 0;
    }
    if (LOG_ON_ERROR((*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_STOPPED))) {
        return -1;
    }
    if (LOG_ON_ERROR((*simple_buffer_queue_)->Clear(simple_buffer_queue_))) {
        return -1;
    }
    initialized_ = false;
    recording_ = false;
    return 0;
}

bool OpenSLESRecorder::ObtainEngineInterface()
{
    ALOGD("ObtainEngineInterface");
    if (engine_)
        return true;

    SLObjectItf engine_object = engine_manager->GetOpenSLEngine();
    if (engine_object == nullptr) {
        ALOGE("Failed to access the global OpenSL engine");
        return false;
    }

    if (LOG_ON_ERROR((*engine_object)->GetInterface(engine_object, SL_IID_ENGINE, &engine_))) {
        return false;
    }
    return true;
}

bool OpenSLESRecorder::CreateAudioRecorder()
{
    ALOGD("CreateAudioRecorder");
    if (recorder_object_.Get())
        return true;
    // Audio source configuration.
    SLDataLocator_IODevice mic_locator = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audio_source = {&mic_locator, NULL};
    // Audio sink configuration.
    SLDataLocator_AndroidSimpleBufferQueue buffer_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, static_cast<SLuint32>(kNumOfOpenSLESBuffers)};
    SLDataSink audio_sink = {&buffer_queue, &pcm_format_};

    const SLInterfaceID interface_id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION};
    const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    if (LOG_ON_ERROR((*engine_)->CreateAudioRecorder(
            engine_, recorder_object_.Receive(), &audio_source, &audio_sink,
            arraysize(interface_id), interface_id, interface_required))) {
        return false;
    }

    SLAndroidConfigurationItf recorder_config;
    if (LOG_ON_ERROR((recorder_object_->GetInterface(recorder_object_.Get(), SL_IID_ANDROIDCONFIGURATION, &recorder_config)))) {
        return false;
    }
    SLint32 stream_type = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;
    if (LOG_ON_ERROR(((*recorder_config)->SetConfiguration(recorder_config, SL_ANDROID_KEY_RECORDING_PRESET, &stream_type, sizeof(SLint32))))) {
        return false;
    }

    if (LOG_ON_ERROR((recorder_object_->Realize(recorder_object_.Get(), SL_BOOLEAN_FALSE)))) {
        return false;
    }
    if (LOG_ON_ERROR((recorder_object_->GetInterface(recorder_object_.Get(), SL_IID_RECORD, &recorder_)))) {
        return false;
    }
    if (LOG_ON_ERROR((recorder_object_->GetInterface(recorder_object_.Get(), SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &simple_buffer_queue_)))) {
        return false;
    }

    if (LOG_ON_ERROR(((*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_, SimpleBufferQueueCallback, this)))) {
        return false;
    }
    return true;
}

void OpenSLESRecorder::DestroyAudioRecorder()
{
    ALOGD("DestroyAudioRecorder");
    if (!recorder_object_.Get())
        return;
    (*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_, nullptr, nullptr);
    recorder_object_.Reset();
    recorder_ = nullptr;
    simple_buffer_queue_ = nullptr;
}

void OpenSLESRecorder::AllocateDataBuffers(int sample_rate, int channles)
{
    ALOGD("AllocateDataBuffers");
    const int buffer_size_samples = sample_rate/50 * channles;  
    audio_buffers_.reset(new std::unique_ptr<SLint16[]>[kNumOfOpenSLESBuffers]);
    for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
        audio_buffers_[i].reset(new SLint16[buffer_size_samples]);
    }
    bytes_per_buffer_ = buffer_size_samples * 2;
}

void OpenSLESRecorder::SimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf buffer_queue, void* context)
{
    OpenSLESRecorder* stream = static_cast<OpenSLESRecorder*>(context);
    stream->ReadBufferQueue();
}

void OpenSLESRecorder::ReadBufferQueue()
{
    SLuint32 state = GetRecordState();
    if (state != SL_RECORDSTATE_RECORDING) {
        ALOGW("Buffer callback in non-recording state!");
        return;
    }

    cap_buffer.write(reinterpret_cast<SLuint8*>(audio_buffers_[buffer_index_].get()), bytes_per_buffer_);

    EnqueueAudioBuffer();
}

bool OpenSLESRecorder::EnqueueAudioBuffer()
{
    SLresult err =(*simple_buffer_queue_)->Enqueue(simple_buffer_queue_,reinterpret_cast<SLint8*>(audio_buffers_[buffer_index_].get()),bytes_per_buffer_);
    if (SL_RESULT_SUCCESS != err) {
        ALOGE("Enqueue failed: %s", GetSLErrorString(err));
        return false;
    }
    buffer_index_ = (buffer_index_ + 1) % kNumOfOpenSLESBuffers;
    return true;
}

SLuint32 OpenSLESRecorder::GetRecordState() const
{
    SLuint32 state;
    SLresult err = (*recorder_)->GetRecordState(recorder_, &state);
    if (SL_RESULT_SUCCESS != err) {
        ALOGE("GetRecordState failed: %s", GetSLErrorString(err));
    }
    return state;
}

SLAndroidSimpleBufferQueueState OpenSLESRecorder::GetBufferQueueState() const
{
    SLAndroidSimpleBufferQueueState state;
    SLresult err = (*simple_buffer_queue_)->GetState(simple_buffer_queue_, &state);
    if (SL_RESULT_SUCCESS != err) {
        ALOGE("GetState failed: %s", GetSLErrorString(err));
    }
    return state;
}

SLuint32 OpenSLESRecorder::GetBufferCount()
{
    SLAndroidSimpleBufferQueueState state = GetBufferQueueState();
    return state.count;
}

void OpenSLESRecorder::LogBufferState() const
{
    SLAndroidSimpleBufferQueueState state = GetBufferQueueState();
    ALOGD("state.count:%d state.index:%d", state.count, state.index);
}