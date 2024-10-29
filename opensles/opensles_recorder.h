
#ifndef __OPENSLES_RECORDER_H__
#define __OPENSLES_RECORDER_H__

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <memory>

#include "opensles_common.h"
#include "kBuffer.h"

class OpenSLESRecorder {
public:
    static const int kNumOfOpenSLESBuffers = 2;

    OpenSLESRecorder();
    ~OpenSLESRecorder();

    int Open(int sample_rate, int channels);
    int Close(void);

    int BytesPerBuffer() const { return bytes_per_buffer_; }
    int Read(uint8_t *data, int length);

    int InitRecording();
    int StartRecording();
    int StopRecording();

private:
    bool ObtainEngineInterface();

    bool CreateAudioRecorder();
    void DestroyAudioRecorder();

    void AllocateDataBuffers(int sample_rate, int channles);

    static void SimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void* context);
    void ReadBufferQueue();
    bool EnqueueAudioBuffer();

    SLuint32 GetRecordState() const;
    SLAndroidSimpleBufferQueueState GetBufferQueueState() const;
    SLuint32 GetBufferCount();

    void LogBufferState() const;

private:
    SLDataFormat_PCM pcm_format_;
    SLEngineItf engine_;
    ScopedSLObjectItf recorder_object_;
    SLRecordItf recorder_;
    SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

    bool initialized_;
    bool recording_;

    int bytes_per_buffer_;

    std::unique_ptr<std::unique_ptr<SLint16[]>[]> audio_buffers_;
    int buffer_index_;

    kBuffer cap_buffer;
};

#endif
