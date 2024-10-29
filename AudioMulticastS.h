#ifndef __AUDIO_MULTICASTS_H__
#define __AUDIO_MULTICASTS_H__

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <memory>
extern "C"
{
#ifdef __cplusplus
#ifndef __STDC_CONSTANT_MACROS
 #define __STDC_CONSTANT_MACROS
 #endif

 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 #include <stdint.h>
#endif

#include "include/libavformat/avformat.h"
#include "include/libswresample/swresample.h"
}
#include <string>
#include "types.h"


#include "opensles_recorder.h"



#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

enum {
    PLAY_STA_STOP = -1,
    PLAY_STA_IDLE=0,
    PLAY_STA_PLAYING,
    PLAY_STA_PAUSE,
};

typedef struct {
    int codec;
    int channel;
    int rate;
    int sample_fmt;
    int vol;
    char url[512];
} Audio_param_t;

class AudioMulticastS {
public:
    AudioMulticastS();
    ~AudioMulticastS();

    int init(int index);
    int createSocket(int port);
    int start(int port, const char * url);
    void stop(void);
    int playMusic(int port, int loop, const char *file);
    void pausePlay(void);
    int open(const char *path);
    void sendProcess(void);

private:

    int read_frame(void);
    int send_frame(int size);
    void end_play(void);

public:
    Audio_param_t k_para;

private:
    int k_socket;
    int k_sta; // 0 idle 1 run 2 pause
    int k_index = 0; 
    int k_port;	 
    int k_shout=0; 
    int k_stream_index;
    int16_t *k_buffer;


    pthread_mutex_t k_mutex;
    AVFormatContext *k_context;
    AVCodecContext *k_avctext;
    OpenSLESRecorder k_recorder;
    SwrContext *k_swctx;



};

extern AudioMulticastS audio_multicast_s[MAX_MULTICAST_CHANNEL];

#endif