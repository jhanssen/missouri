#include "Encoder.h"
#include "UdpSocket.h"
#include <pthread.h>
#include <sched.h>
extern "C" {
#include <x264.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

class EncoderPrivate
{
public:
    const uint8_t* input;
    int32_t width, height;
    int32_t size;

    uint8_t* output;
    int32_t outputSize;

    x264_t* encoder;
    x264_picture_t pic_in, pic_out;

    int headerSize;
    uint8_t* sps;
    int spsSize;
    uint8_t* pps;
    int ppsSize;

    SwsContext* scale;

    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t encodeCond, doneCond;
    bool encoding;

    static void* run(void* arg);
};

static inline void stream_frame(AVFormatContext* avctx, uint8_t* payload, int size)
{
    // initalize a packet
    AVPacket p;
    av_init_packet(&p);
    p.data = payload;
    p.size = size;
    p.stream_index = 0;
    p.flags = AV_PKT_FLAG_KEY;
    p.pts = AV_NOPTS_VALUE;
    p.dts = AV_NOPTS_VALUE;

    // send it out
    av_interleaved_write_frame(avctx, &p);
}

void* EncoderPrivate::run(void* arg)
{
    //Host host("192.168.11.120", 27584);
    //UdpSocket socket;

    av_register_all();
    avformat_network_init();

    AVFormatContext* avctx = avformat_alloc_context();
    AVOutputFormat* fmt = av_guess_format("rtp", NULL, NULL);
    avctx->oformat = fmt;

    snprintf(avctx->filename, sizeof(avctx->filename), "rtp://%s:%d", "192.168.11.120", 27584);
    if (avio_open(&avctx->pb, avctx->filename, AVIO_FLAG_WRITE) < 0)
        printf("Unable to open URL\n");

    AVStream* stream = av_new_stream(avctx, 1);

    // initalize codec
    AVCodecContext* c = stream->codec;
    c->codec_id = CODEC_ID_H264;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->flags = CODEC_FLAG_GLOBAL_HEADER;
    c->width = 1440;
    c->height = 900;
    c->time_base.den = 60;
    c->time_base.num = 1;
    c->gop_size = 60;
    c->bit_rate = 400000;
    avctx->flags = 0; //AVFMT_FLAG_RTP_HINT;

    // write the header
    avformat_write_header(avctx, 0);

    EncoderPrivate* priv = static_cast<EncoderPrivate*>(arg);
    const int32_t w = priv->width;
    const int32_t h = priv->height;
    int frame = 0;
    for (;;) {
        pthread_mutex_lock(&priv->mutex);
        while (!priv->encoding) {
            pthread_cond_wait(&priv->encodeCond, &priv->mutex);
        }
        pthread_mutex_unlock(&priv->mutex);

        int srcstride = w * 3; // RGB stride is 3 * width
        sws_scale(priv->scale, &priv->input, &srcstride, 0, h, priv->pic_in.img.plane, priv->pic_in.img.i_stride);
        x264_nal_t* nals;
        int i_nals;
        const int frame_size = x264_encoder_encode(priv->encoder, &nals, &i_nals, &priv->pic_in, &priv->pic_out);
        if (frame_size >= 0) {
            //printf("frame %d, size %d\n", frame, frame_size);
            ++frame;

            for (int i = 0; i < i_nals; ++i) {
                //const int packetSize = nals[i].i_payload - 4; // ### right?
                //const uint8_t* payload = nals[i].p_payload + 4;
                //printf("nal %d (%d %p)\n", i, packetSize, payload);

                stream_frame(avctx, nals[i].p_payload, nals[i].i_payload);
                //socket.send(host, reinterpret_cast<const char*>(payload), packetSize);
            }
        } else {
            fprintf(stderr, "bad frame!\n");
        }

        pthread_mutex_lock(&priv->mutex);
        priv->encoding = false;
        pthread_cond_signal(&priv->doneCond);
        pthread_mutex_unlock(&priv->mutex);
        sched_yield();
    }

    return 0;
}

Encoder::Encoder(const uint8_t* buffer, int32_t width, int32_t height, int32_t size)
    : mPriv(new EncoderPrivate)
{
    mPriv->input = buffer;
    mPriv->width = width;
    mPriv->height = height;
    mPriv->size = size;

    x264_param_t param;
    x264_param_default_preset(&param, "veryfast", "zerolatency");

    param.i_threads = 1;
    param.i_width = width;
    param.i_height = height;
    param.i_fps_num = 60;
    param.i_fps_den = 1;
    //param.i_slice_max_size = 1400;

    // Intra refres:
    param.i_keyint_max = 60;
    param.b_intra_refresh = 1;

    // Rate control:
    param.rc.i_rc_method = X264_RC_ABR;
    param.rc.i_bitrate = 400000;

    param.b_repeat_headers = 1;
    param.b_annexb = 1;

    x264_param_apply_profile(&param, "baseline");

    mPriv->encoder = x264_encoder_open(&param);

    /*
    p_nal = headers
    int sps_size = p_nal[0].i_payload - 4;
    int pps_size = p_nal[1].i_payload - 4;
    int sei_size = p_nal[2].i_payload;

    uint8_t *sps = p_nal[0].p_payload + 4;
    uint8_t *pps = p_nal[1].p_payload + 4;
    uint8_t *sei = p_nal[2].p_payload;
    */
    x264_nal_t* p_nal;
    int i_nal;
    mPriv->headerSize = x264_encoder_headers(mPriv->encoder, &p_nal, &i_nal);

    if (i_nal != 3 || p_nal[0].i_type != NAL_SPS || p_nal[1].i_type != NAL_PPS
        || p_nal[0].i_payload < 4 || p_nal[1].i_payload < 1) {
        fprintf(stderr, "Incorrect x264 header!\n");
        exit(0);
    }

#if (X264_BUILD < 76)
# error x264 version too old
#endif

    mPriv->spsSize = p_nal[0].i_payload - 4;
    mPriv->sps = new uint8_t[mPriv->spsSize];
    memcpy(mPriv->sps, p_nal[0].p_payload + 4, mPriv->spsSize);

    mPriv->ppsSize = p_nal[1].i_payload - 4;
    mPriv->pps = new uint8_t[mPriv->ppsSize];
    memcpy(mPriv->pps, p_nal[1].p_payload + 4, mPriv->ppsSize);

    x264_picture_alloc(&mPriv->pic_in, X264_CSP_I420, width, height);

    mPriv->scale = sws_getContext(width, height, PIX_FMT_RGB24, 1440, 900, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

    mPriv->encoding = false;
    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_cond_init(&mPriv->encodeCond, 0);
    pthread_cond_init(&mPriv->doneCond, 0);
    pthread_create(&mPriv->thread, 0, EncoderPrivate::run, mPriv);
}

Encoder::~Encoder()
{
    pthread_cond_destroy(&mPriv->encodeCond);
    pthread_cond_destroy(&mPriv->doneCond);
    pthread_mutex_destroy(&mPriv->mutex);

    delete[] mPriv->sps;
    delete[] mPriv->pps;

    delete mPriv;
}

void Encoder::getSps(uint8_t** payload, int* size)
{
    *payload = mPriv->sps;
    *size = mPriv->spsSize;
}

void Encoder::getPps(uint8_t** payload, int* size)
{
    *payload = mPriv->pps;
    *size = mPriv->ppsSize;
}

int Encoder::headerSize() const
{
    return mPriv->headerSize;
}

void Encoder::encode()
{
    pthread_mutex_lock(&mPriv->mutex);
    while (mPriv->encoding) {
        pthread_cond_wait(&mPriv->doneCond, &mPriv->mutex);
    }
    mPriv->encoding = true;
    pthread_cond_signal(&mPriv->encodeCond);
    pthread_mutex_unlock(&mPriv->mutex);
}

const uint8_t* Encoder::outputBuffer() const
{
    return mPriv->output;
}

uint32_t Encoder::outputSize() const
{
    return mPriv->outputSize;
}
