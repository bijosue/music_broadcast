#include "AudioMulticastS.h"
#include "include/libavutil/samplefmt.h"
#include "include/libavcodec/avcodec.h"
#include "include/types.h"
#include "include/libavutil/frame.h"

#include "include/common.h"



extern int jni_event_request(const char *url, const char *data);

static void handle_sleep(int runSta, uint16_t ms)
{
    uint16_t idx = ms/10;
    uint16_t offset = ms%10;
    while(runSta > PLAY_STA_IDLE && idx > 0)
    {
        usleep(10*1000);
        idx --;
    }
    if (runSta > PLAY_STA_IDLE && offset > 0)
    {
        usleep(offset*1000);
    }
}

AudioMulticastS audio_multicast_s[MAX_MULTICAST_CHANNEL];

static void *send_audio_process(void *param)
{
    AudioMulticastS *s = (AudioMulticastS *)param;

    s->sendProcess();

    return NULL;
}



AudioMulticastS::AudioMulticastS()
    :k_socket(-1),k_sta(PLAY_STA_IDLE)
{
    k_buffer = (int16_t *)(new char[AVCODEC_MAX_AUDIO_FRAME_SIZE]);

    k_swctx = NULL;
    k_context = NULL;
    k_avctext = NULL;
    memset(k_para.url, 0, sizeof(k_para.url));
    pthread_mutex_init(&k_mutex, NULL);
}

AudioMulticastS::~AudioMulticastS()
{
    this->stop();
    if (k_buffer != NULL) {
        delete [] k_buffer;
        k_buffer = NULL;
    }

}

int AudioMulticastS::init(int index)
{

    if (SHOUT_MULTICAST_INDEX == index)
    {
        k_shout = 1;
    }
    k_index = index;

    return 0;
}

int AudioMulticastS::createSocket(int port)
{


    if((k_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        KLOGE("createSocket socket create error\n");
        return -1;
    }

    struct sockaddr_in addr;
    struct ip_mreq mreq;

    bzero(&addr, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);



    int so_broadcast = 1;
    setsockopt(k_socket, SOL_SOCKET, SO_BROADCAST,
               &so_broadcast, sizeof(so_broadcast));


    if (bind(k_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        KLOGE("createSocket bind error\n");
        return -1;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(AUDIO_MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(k_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                  &mreq, sizeof(mreq)) < 0) {
        KLOGE("createSocket IP_ADD_MEMBERSHIP error\n");
        return -1;
    }

    int loop = 0;
    setsockopt(k_socket, IPPROTO_IP, IP_MULTICAST_LOOP,
               &loop, sizeof(loop));

    k_port = port;
    return 0;
}

int AudioMulticastS::start(int port, const char * url)
{
    switch (k_sta)
    {
        case PLAY_STA_IDLE:
            goto START;
            break;
        case PLAY_STA_PLAYING:
            goto STOP;
            break;
        case PLAY_STA_PAUSE:
            if(url && strcmp(url, k_para.url))
            {
                KLOGI("old url:%s  new url:%s\n", k_para.url, url);
                memset(k_para.url, 0, sizeof(k_para.url));
                memcpy(k_para.url, url, sizeof(k_para.url));
                goto STOP;
            }
            k_sta = PLAY_STA_PLAYING;
            break;
        default:

            break;
    }

    return 0;
STOP:
    this->stop();
    usleep(1000*100);

START:

    if(this->createSocket(port) == 0)
    {
        if (this->open(url) == 0)
        {
            pthread_t pid;
            if (pthread_create(&pid, NULL, send_audio_process, this) != 0)
            {
                KLOGE("AudioMulticastS pthread_create error.\n");

            }
            k_sta = PLAY_STA_PLAYING;
        }else
        {
            if (k_socket > 0)
            {
                ::close(k_socket);
                k_socket = -1;
            }
            return -1;
        }


    }else
    {
        return -1;
    }
    return 0;


}

void AudioMulticastS::stop(void)
{
    k_sta  = PLAY_STA_IDLE;
}

int AudioMulticastS::playMusic(int port, int loop, const char *file) {
    if (1 == loop)
    {
        if (PLAY_STA_PLAYING <= k_sta)
        {
            return 0;
        }
    }
    return this->start(port, file);

}

void AudioMulticastS::pausePlay() {
    if (PLAY_STA_PLAYING == k_sta)
    {
        k_sta = PLAY_STA_PAUSE;
    }
}




int AudioMulticastS::open(const char *path) {
    if (!path)
    {
        return -1;
    }
    if(!strcmp(path, "/dev/mic")) {

        k_recorder.Open(44100, 1);

        k_shout = 1;
        k_para.channel = 1;
        k_para.rate = 44100;
        k_para.codec = AV_CODEC_ID_PCM_S16LE;
        k_para.sample_fmt = AV_SAMPLE_FMT_S16;
        memset(k_para.url, 0, sizeof(k_para.url));
        strcpy(k_para.url, path);
        return 0;
    }else {
        struct stat st;
        if (stat(path, &st))
        {
            KLOGE("can't find file %s\n", path);
            return -1;
        }
        int ret;
        if ((ret = avformat_open_input(&k_context, path, NULL, NULL)) < 0)
        {
            KLOGE("open (%s) error (%d)\n", path, ret);
            return -1;
        }
     //   KLOGI("stremas nb(%d) \n", k_context->nb_streams);
        if (avformat_find_stream_info(k_context, NULL) < 0)
        {
            KLOGE("avformat_find_stream_info error \n");
            goto FAILED;
        }
     //   av_dump_format(k_context, 0, path, 0);
        AVCodec *codec;
        k_stream_index = av_find_best_stream(k_context, AVMEDIA_TYPE_AUDIO, -1, -1,
                                             &codec, 0);
        if (k_stream_index < 0)
        {
            KLOGE("can't find best audio stream.\n");
            goto FAILED;
        }
        k_avctext = avcodec_alloc_context3(codec);
        if (!k_avctext)
        {
            goto FAILED;
        }
    

        if (avcodec_parameters_to_context(k_avctext,
                                          k_context->streams[k_stream_index]->codecpar) < 0)
        {
            goto FAILED;
        }

        if (avcodec_open2(k_avctext, codec, NULL) < 0)
        {
            goto FAILED;
        }

        if (k_avctext->sample_fmt != AV_SAMPLE_FMT_S16)
        {
            k_swctx = swr_alloc_set_opts(NULL, k_avctext->channel_layout,
                                         AV_SAMPLE_FMT_S16, k_avctext->sample_rate,
                                         k_avctext->channel_layout, k_avctext->sample_fmt,
                                         k_avctext->sample_rate, 0, NULL);
            if (!k_swctx)
            {
                goto FAILED;
            }
            swr_init(k_swctx);
        }

        if(k_avctext->channels < 1 || k_avctext->channels > 2 )
        {
            KLOGE("unSupport channel:%d\n", k_avctext->channels);
            goto FAILED;
        }
        if (k_avctext->sample_rate != 8000 &&
            k_avctext->sample_rate != 11025 &&
            k_avctext->sample_rate !=16000 &&
            k_avctext->sample_rate !=22050 &&
            k_avctext->sample_rate !=32000 &&
            k_avctext->sample_rate !=44100 &&
            k_avctext->sample_rate !=48000 &&
            k_avctext->sample_rate !=96000 &&
            k_avctext->sample_rate !=192000)
        {
            KLOGE("unSupport rate:%d\n", k_avctext->sample_rate);
            goto FAILED;
        }

        memset(k_para.url, 0, sizeof(k_para.url));
        strcpy(k_para.url, path);
//        k_para.codec = k_avctext->codec_id;
        k_para.codec = AV_CODEC_ID_PCM_S16LE;
        k_para.sample_fmt = AV_SAMPLE_FMT_S16;
        k_para.channel = k_avctext->channels;
        k_para.rate = k_avctext->sample_rate;
        KLOGI("open file:%s success\n channel:%d rate:%d\n", k_para.url,
              k_para.channel, k_para.rate);
        return 0;


FAILED:
        if (k_avctext)
        {
            avcodec_free_context(&k_avctext);
            k_avctext = NULL;
        }
        if (k_context)
        {
            avformat_close_input(&k_context);
            k_context = NULL;
        }
        return -1;
    }
    return -1;
}

void AudioMulticastS::sendProcess(void) {

    int  ret = 0, err = 0, offset = 2;
    long int rByte = 0;
    int Bps = sizeof(int16_t)*k_para.rate*k_para.channel;
    uint64_t start = getUpTimeSec();
    uint64_t cur_index = 0;
    uint32_t rTime = 0;

    rByte = Bps;
    rTime = offset;
    KLOGI("start multicast at(%llu) Bps(%d)\n", start, Bps);
    while(PLAY_STA_IDLE < k_sta)
    {
        if (PLAY_STA_PAUSE == k_sta)
        {

            start = getUpTimeSec();
            rByte = Bps;
            rTime = offset;

            handle_sleep(k_sta, 40);
            continue;
        }


        cur_index = (int)llabs((long long)(getUpTimeSec() - start));
        if (cur_index + offset <= rTime)
        {
            handle_sleep(k_sta, 40);
        }
        if (k_sta <= PLAY_STA_IDLE) break;
        usleep(1000*20);
        ret = read_frame();
        if (ret > 0)
        {
            if (k_sta <= PLAY_STA_IDLE) break;

            this->send_frame(ret);
            rByte += ret;
            rTime = rByte/Bps;
        }else if (ret < 0)
        {
            if (++err >= 20)
            {
                KLOGE("muiticast thread break.\n");
                break;
            }

        }else
        {
            if (0 == k_shout)
                break;
        }

    }

    this->end_play();
    KLOGI("muiticast thread end.\n");

}

int AudioMulticastS::read_frame(void) {
    int ret = 0;

    if (k_shout)
    {
        int len = k_recorder.BytesPerBuffer();
        ret = k_recorder.Read((uint8_t *)k_buffer, len);
 //       KLOGI("mic read (%d)\n", ret);
        return ret;
    }

    AVPacket pkt;
    AVFrame *frame = NULL;

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    int date_size = 0, total_size = 0;
    if (k_context && k_avctext) {
        ret = av_read_frame(k_context, &pkt);
        if (ret == 0)
        {
            frame = av_frame_alloc();
            //decode
            if (pkt.stream_index != k_stream_index)
            {
                goto FAILED;
            }
//            KLOGI("read a frame pkt size:%d\n", pkt.size);
            if (avcodec_send_packet(k_avctext, (const AVPacket *)&pkt) < 0)
            {
                goto FAILED;
            }

            while ((ret = avcodec_receive_frame(k_avctext, frame)) == 0)
            {
                date_size = sizeof(int16_t) *frame->nb_samples;
                if (k_swctx)
                {
                    int buffer_size = av_samples_get_buffer_size(NULL, frame->channels, frame->nb_samples,
                                                                 AV_SAMPLE_FMT_S16, 0);
                    if (buffer_size > 0)
                    {
                        uint8_t *buf = (uint8_t *) av_malloc(buffer_size);
                        int out_nb_sample = swr_convert(k_swctx, &buf, buffer_size,
                                                        (const uint8_t **)&frame->data,frame->nb_samples);
                        if (out_nb_sample < 0)
                        {
                            KLOGE( "Failed to swr_convert .\n");
                            av_free(buf);
                            buf = NULL;
                            goto FAILED;
                        }
                        else
                        {
                            memcpy(k_buffer+total_size, buf, buffer_size);
                            total_size += buffer_size;
 //                           KLOGI("frame buffer size:%d\n", total_size);
                        }
                        av_free(buf);
                        buf = NULL;
                    }

                }else
                {
                    memcpy(k_buffer+total_size, frame->data[0], date_size);
                    total_size += date_size;
                }
                av_frame_unref(frame);
            }

        }else
        {
            if (ret == AVERROR_EOF)
            {
                av_packet_unref(&pkt);
                return 0;
            }
        }
    }

    if (frame)
    {
        av_frame_free(&frame);
        frame = NULL;
    }
    av_packet_unref(&pkt);
    return total_size;

FAILED:
    if (frame)
    {
        av_frame_free(&frame);
        frame = NULL;
    }
    av_packet_unref(&pkt);
    return -1;

}

int AudioMulticastS::send_frame(int size) {
    if (size == 0)
    {
        return 0;
    }
    if (k_socket < 0 || size < 0)
    {
        return -1;
    }
    struct sockaddr_in addr;
    int ret = -1;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(AUDIO_MULTICAST_ADDR);
    addr.sin_port = htons(k_port);
    ret = sendto(k_socket, (const char*)k_buffer, size, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr));
    return ret;

}

void AudioMulticastS::end_play() {
    if (k_shout)
    {
        k_recorder.Close();
    }else
    {
        if (k_context)
        {
            if (k_swctx)
            {
                swr_free(&k_swctx);

            }
           // avcodec_close(k_avctext);
            avcodec_free_context(&k_avctext);
            k_avctext = NULL;
            avformat_close_input(&k_context);
            k_context = NULL;

        }
    }
    if (k_socket > 0)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(AUDIO_MULTICAST_ADDR);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(k_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        {
            KLOGE("broadcast socket setsockopt IP_DROP_MEMBERSHIP error:");

        }
        ::close(k_socket);
        k_socket = -1;
    }
    if (PLAY_STA_PLAYING == k_sta)
    {
       
        k_sta  = PLAY_STA_IDLE;
    }
}
