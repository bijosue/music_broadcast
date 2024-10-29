#ifndef __AUDIO_MULTICASTRECV_H__
#define __AUDIO_MULTICASTRECV_H__

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


#include "opensles/opensles_player.h"



class AudioMulticastR {
public:
    AudioMulticastR();
    ~AudioMulticastR();

    int start(void);
    int setParam(int port, int channels,
                 int rate, int index, int vol);
    int createSocket(void);
    int stop(int);
    void stopReceive(int index);
    void process(void);
    void close(void);

private:
    int k_socket;
    int k_port;
    int k_channels;
    int k_rate;
    bool k_start;
    int k_index;
    int k_vol;

    OpenSLESPlayer k_player;
};

extern AudioMulticastR audio_multicast_r;

#endif