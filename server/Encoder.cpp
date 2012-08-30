#include "Encoder.h"
#include "UdpSocket.h"
#include <pthread.h>
#include <sched.h>
extern "C" {
#include <x264.h>
#include <libswscale/swscale.h>
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

    x264_nal_t *headers;
    int i_nal;

    SwsContext* scale;

    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t encodeCond, doneCond;
    bool encoding;

    static void* run(void* arg);
};

void* EncoderPrivate::run(void* arg)
{
    Host host("192.168.11.120", 27584);
    UdpSocket socket;

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
                const int packetSize = nals[i].i_payload;
                const uint8_t* payload = nals[i].p_payload;
                //printf("nal %d (%d %p)\n", i, packetSize, payload);

                socket.send(host, reinterpret_cast<const char*>(payload), packetSize);
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
    param.i_slice_max_size = 1400;

    // Intra refres:
    param.i_keyint_max = 60;
    param.b_intra_refresh = 1;

    // Rate control:
    param.rc.i_rc_method = X264_RC_CRF;
    param.rc.f_rf_constant = 25;
    param.rc.f_rf_constant_max = 35;

    x264_param_apply_profile(&param, "baseline");

    mPriv->encoder = x264_encoder_open(&param);

    x264_encoder_headers(mPriv->encoder, &mPriv->headers, &mPriv->i_nal);
    /*
    p_nal = headers
    int sps_size = p_nal[0].i_payload - 4;
    int pps_size = p_nal[1].i_payload - 4;
    int sei_size = p_nal[2].i_payload;

    uint8_t *sps = p_nal[0].p_payload + 4;
    uint8_t *pps = p_nal[1].p_payload + 4;
    uint8_t *sei = p_nal[2].p_payload;
    */

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
    delete mPriv;
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
