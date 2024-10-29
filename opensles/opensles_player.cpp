#include <android/log.h>

#include "opensles_player.h"
#include "audio_manager.h"

#include "arraysize.h"
#include "types.h"

#define TAG "OpenSLESPlayer"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#define RETURN_ON_ERROR(op, ...)                          \
  do {                                                    \
    SLresult err = (op);                                  \
    if (err != SL_RESULT_SUCCESS) {                       \
      ALOGE("%s failed: %s", #op, GetSLErrorString(err)); \
      return __VA_ARGS__;                                 \
    }                                                     \
  } while (0)

OpenSLESPlayer::OpenSLESPlayer()
{
    initialized_ = false;
    playing_ = false;
    buffer_index_ = 0;
    engine_ = nullptr;
    player_ = nullptr;
    simple_buffer_queue_ = nullptr;
    plbk_buffer.realloc(200*1024);  
}

OpenSLESPlayer::~OpenSLESPlayer()
{
    this->Close();
    engine_ = NULL;
}

int OpenSLESPlayer::Open(int sample_rate, int channels)
{
    plbk_buffer.reset();

    pcm_format_ = CreatePCMConfiguration(sample_rate, channels, SL_PCMSAMPLEFORMAT_FIXED_16);
    AllocateDataBuffers(sample_rate, channels);

    KLOGI("OpenslesPlayer::open rate=%d channel=%d period_bytes=%d\n", sample_rate, channels, bytes_per_buffer_);
    InitPlayout();
    StartPlayout();
    return playing_ ? 0 : -1;
}

int OpenSLESPlayer::Close()
{
    plbk_buffer.reset();
    StopPlayout();
    DestroyMix();
    engine_ = nullptr;
    return 0;
}

int OpenSLESPlayer::InitPlayout()
{
    ALOGD("InitPlayout");
    if (initialized_ || playing_)
        return -1;
    if (!ObtainEngineInterface()) {
        ALOGE("Failed to obtain SL Engine interface");
        return -1;
    }
    CreateMix();
    initialized_ = true;
    buffer_index_ = 0;
    return 0;
}

int OpenSLESPlayer::StartPlayout()
{
    ALOGD("StartPlayout[");
    if (!initialized_ || playing_)
        return -1;

    CreateAudioPlayer();
    for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
        EnqueuePlayoutData(true);
    }
    RETURN_ON_ERROR((*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING), -1);
    playing_ = (GetPlayState() == SL_PLAYSTATE_PLAYING);
    return (playing_ ? 0 : -1);
}

int OpenSLESPlayer::StopPlayout()
{
    ALOGD("StopPlayout");
    if (!initialized_ || !playing_) {
        return 0;
    }
    RETURN_ON_ERROR((*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED), -1);
    RETURN_ON_ERROR((*simple_buffer_queue_)->Clear(simple_buffer_queue_), -1);

    DestroyAudioPlayer();
    initialized_ = false;
    playing_ = false;
    return 0;
}

int OpenSLESPlayer::Write(uint8_t *data, int length)
{
    int ret = plbk_buffer.write(data, length);
    if  (ret == 0)
        plbk_buffer.reset();
    return ret;
}

void OpenSLESPlayer::SimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void* context)
{
    OpenSLESPlayer* player = reinterpret_cast<OpenSLESPlayer*>(context);
    player->FillBufferQueue();
}

void OpenSLESPlayer::FillBufferQueue()
{
    SLuint32 state = GetPlayState();
    if (state != SL_PLAYSTATE_PLAYING) {
        ALOGW("Buffer callback in non-playing state!");
        return;
    }
    EnqueuePlayoutData(false);
}

void OpenSLESPlayer::EnqueuePlayoutData(bool silence)
{
    SLuint8 *audio_ptr8 = reinterpret_cast<SLuint8*>(audio_buffers_[buffer_index_].get());
    if (silence) {
        memset(audio_ptr8, 0, bytes_per_buffer_);
    } else {
        if (plbk_buffer.availd() < bytes_per_buffer_) {
            memset(audio_ptr8, 0, bytes_per_buffer_);
        } else {
            plbk_buffer.read(audio_ptr8, bytes_per_buffer_);
        }
    }

    SLresult err = (*simple_buffer_queue_)->Enqueue(simple_buffer_queue_, audio_ptr8, bytes_per_buffer_);
    if (SL_RESULT_SUCCESS != err) {
        ALOGE("Enqueue failed: %d", err);
    }
    buffer_index_ = (buffer_index_ + 1) % kNumOfOpenSLESBuffers;
}

void OpenSLESPlayer::AllocateDataBuffers(int sample_rate, int channles)
{
    ALOGD("AllocateDataBuffers");
    const int buffer_size_samples = sample_rate/50 * channles; 
    for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
        audio_buffers_[i].reset(new SLint16[buffer_size_samples]);
    }
    bytes_per_buffer_ = buffer_size_samples * 2;
}

bool OpenSLESPlayer::ObtainEngineInterface()
{
    ALOGD("ObtainEngineInterface");
    if (engine_)
        return true;

    SLObjectItf engine_object = engine_manager->GetOpenSLEngine();
    if (engine_object == nullptr) {
        ALOGE("Failed to access the global OpenSL engine");
        return false;
    }

    RETURN_ON_ERROR((*engine_object)->GetInterface(engine_object, SL_IID_ENGINE, &engine_), false);
    return true;
}

bool OpenSLESPlayer::CreateMix()
{
    ALOGD("CreateMix");
    if (output_mix_.Get())
        return true;
    RETURN_ON_ERROR((*engine_)->CreateOutputMix(engine_, output_mix_.Receive(), 0, nullptr, nullptr), false);
    RETURN_ON_ERROR(output_mix_->Realize(output_mix_.Get(), SL_BOOLEAN_FALSE), false);
    return true;
}

void OpenSLESPlayer::DestroyMix()
{
    ALOGD("DestroyMix");
    if (!output_mix_.Get())
        return;
    output_mix_.Reset();
}

bool OpenSLESPlayer::CreateAudioPlayer()
{
    ALOGD("CreateAudioPlayer");
    if (player_object_.Get())
        return true;
    SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, static_cast<SLuint32>(kNumOfOpenSLESBuffers)};
    SLDataSource audio_source = {&simple_buffer_queue, &pcm_format_};
    SLDataLocator_OutputMix locator_output_mix = {SL_DATALOCATOR_OUTPUTMIX, output_mix_.Get()};
    SLDataSink audio_sink = {&locator_output_mix, NULL};
    const SLInterfaceID interface_ids[] = {SL_IID_ANDROIDCONFIGURATION, SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    RETURN_ON_ERROR((*engine_)->CreateAudioPlayer(engine_, player_object_.Receive(), &audio_source, &audio_sink, arraysize(interface_ids), interface_ids, interface_required), false);

    SLAndroidConfigurationItf player_config;
    RETURN_ON_ERROR(player_object_->GetInterface(player_object_.Get(), SL_IID_ANDROIDCONFIGURATION, &player_config), false);
    SLint32 stream_type = SL_ANDROID_STREAM_MEDIA;  
    RETURN_ON_ERROR((*player_config)->SetConfiguration(player_config, SL_ANDROID_KEY_STREAM_TYPE, &stream_type, sizeof(SLint32)), false);

    RETURN_ON_ERROR(player_object_->Realize(player_object_.Get(), SL_BOOLEAN_FALSE), false);
    RETURN_ON_ERROR(player_object_->GetInterface(player_object_.Get(), SL_IID_PLAY, &player_), false);

    RETURN_ON_ERROR(player_object_->GetInterface(player_object_.Get(), SL_IID_BUFFERQUEUE, &simple_buffer_queue_), false);

    
    RETURN_ON_ERROR((*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_, SimpleBufferQueueCallback, this), false);

    return true;
}

void OpenSLESPlayer::DestroyAudioPlayer()
{
    ALOGD("DestroyAudioPlayer");
    if (!player_object_.Get())
        return;
    (*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_, nullptr, nullptr);
    player_object_.Reset();
    player_ = nullptr;
    simple_buffer_queue_ = nullptr;
}

SLuint32 OpenSLESPlayer::GetPlayState() const
{
    SLuint32 state;
    SLresult err = (*player_)->GetPlayState(player_, &state);
    if (SL_RESULT_SUCCESS != err) {
        ALOGE("GetPlayState failed: %d", err);
    }
    return state;
}