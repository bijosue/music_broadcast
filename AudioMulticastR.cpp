#include "AudioMulticastR.h"
#include "common.h"


AudioMulticastR audio_multicast_r;

static void *receive_audio(void *params)
{
    if (!params)
    {
        return NULL;
    }
    AudioMulticastR *audio_multicast = (AudioMulticastR *)params;
    audio_multicast->process();
    return NULL;
}

AudioMulticastR::AudioMulticastR()
{
    k_start = false;
    k_index = -1;
    k_socket = -1;

}

AudioMulticastR::~AudioMulticastR()
{
    this->close();
}

int AudioMulticastR::start(void)
{

    if (k_start)
    {
        return -1;
    }


    if (this->createSocket() < 0)
    {
        return -3;
    }

    pthread_t pid;
    if (pthread_create(&pid, NULL, receive_audio, this) != 0)
    {
        KLOGE("AudioBroadcast pthread_create\n");
        return -4;
    }
    k_start = true;
    return 0;

}

int AudioMulticastR::setParam(int port, int channels,
                              int rate, int index, int vol)
{
    k_vol = vol;

    if (k_start)
    {
        return -1;
    }
    k_port = port;
    k_channels = channels;
    k_rate = rate;
    k_index = index;


    return 0;
}

int AudioMulticastR::createSocket(void)
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
    addr.sin_port = htons(k_port);
    int snd_size = (200*1024);
    int optlen = sizeof(snd_size);

    setsockopt(k_socket, SOL_SOCKET,SO_RCVBUF, (char *)&snd_size, optlen);

    if (bind(k_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        KLOGE("broadcast socket bind error:");
        return -2;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(AUDIO_MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(k_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        KLOGE("broadcast socket setsockopt IP_ADD_MEMBERSHIP error:");
        return -3;
    }

    return 0;
}

int AudioMulticastR::stop(int index) {

    if (index == -1) {
        k_start = false;
    }else if (index == k_index)
    {
        k_start = false;
    }
    return 0;
}

void AudioMulticastR::stopReceive(int index) {
    if (k_start && k_index == index)
    {
        k_start = false;
    }
}

void AudioMulticastR::process() {
    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 40*1000;

    char buf[1024*32];
    struct sockaddr addr;
    socklen_t addr_len;
    addr_len = sizeof(struct sockaddr_in);
    uint64_t recv_time = getUpTimeSec();
    if (k_player.Open(k_rate, k_channels))
    {
        KLOGE("AudioMulticastR player open error.\n");
        return;
    }

    int nbytes;
    while(k_start)
    {
        FD_ZERO(&rfds);

        FD_SET(k_socket, &rfds);


        select(k_socket+1, &rfds, NULL, NULL, &tv);
        if (llabs((long long)(getUpTimeSec() - recv_time)) > 30)
        {
            break;
        }
        if (FD_ISSET(k_socket, &rfds))
        {
            nbytes = recvfrom(k_socket, buf, sizeof(buf), 0, &addr, &addr_len);
            if (nbytes > 0)
            {
                recv_time = getUpTimeSec();
                if (k_vol >= 0)
                {
                    short pcmval;
                    for (int i = 0; i < nbytes; )
                    {
                        pcmval = (buf[i+1] << 8) | buf[i];
                        pcmval = (pcmval*(k_vol/5))>>6;
                        if (pcmval > 32767)
                            pcmval = 32767;
                        else if (pcmval < -32768)
                            pcmval = -32768;

                        buf[i] = (uint8_t)(pcmval&0xFF);
                        buf[i+1] = (uint8_t)((pcmval >> 8)&0xFF);
                        i += 2;
                    }
                }
                k_player.Write((uint8_t*)buf, nbytes);
            }
        }

    }

    KLOGI("stop multicast recv.\n");

    k_start = false;
    this->close();
}

void AudioMulticastR::close() {
    if (k_socket)
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
    k_player.Close();
}
