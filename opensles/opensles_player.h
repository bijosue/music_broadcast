#ifndef __OPENSLES_PLAYER_H__
#define __OPENSLES_PLAYER_H__

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <memory>

#include "opensles_common.h"
#include "kBuffer.h"

class OpenSLESPlayer {
public:
    static const int kNumOfOpenSLESBuffers = 8;

    OpenSLESPlayer();
    ~OpenSLESPlayer();

    int Open(int sample_rate, int channels);
    int Close(void);

    int InitPlayout();
    int StartPlayout();
    int StopPlayout();

    int BytesPerBuffer() const { return bytes_per_buffer_; }
    int Write(uint8_t *data, int length);

private:
    static void SimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void* context);
    void FillBufferQueue();
    void EnqueuePlayoutData(bool silence);

    void AllocateDataBuffers(int sample_rate, int channles);

    bool ObtainEngineInterface();

    bool CreateMix();
    void DestroyMix();
    bool CreateAudioPlayer();
    void DestroyAudioPlayer();

    SLuint32 GetPlayState() const;

private:
    SLDataFormat_PCM pcm_format_;
    SLEngineItf engine_;
    ScopedSLObjectItf output_mix_;
    ScopedSLObjectItf player_object_;
    SLPlayItf player_;
    SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

    bool initialized_;
    bool playing_;

    std::unique_ptr<SLint16[]> audio_buffers_[kNumOfOpenSLESBuffers];
    int buffer_index_;

    int bytes_per_buffer_;
    kBuffer plbk_buffer;
};

#endif