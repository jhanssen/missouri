#include "Encoder.h"
#include "UdpSocket.h"
#include <vector>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
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

    int outputWidth, outputHeight;
    std::vector<Host> destinations;

    bool stopped;
};

static inline void sendHeader(const Host& host, UdpSocket& socket, uint32_t nalCount)
{
    static uint64_t header = 0xbeeffeed00000000LL;
    //printf("sending %u nals\n", nalCount);
    header &= 0xffffffff00000000LL;
    header |= static_cast<uint64_t>(nalCount);
    socket.send(host, reinterpret_cast<char*>(&header), 8);
}

void* EncoderPrivate::run(void* arg)
{
    UdpSocket socket;

    EncoderPrivate* priv = static_cast<EncoderPrivate*>(arg);
    const int32_t w = priv->width;
    const int32_t h = priv->height;
    int frame = 0;
    for (;;) {
        pthread_mutex_lock(&priv->mutex);
        while (!priv->encoding) {
            if (priv->stopped) {
                pthread_mutex_unlock(&priv->mutex);
                return 0;
            }
            pthread_cond_wait(&priv->encodeCond, &priv->mutex);
            if (priv->stopped) {
                pthread_mutex_unlock(&priv->mutex);
                return 0;
            }
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

            pthread_mutex_lock(&priv->mutex);
            std::vector<Host>::const_iterator it = priv->destinations.begin();
            const std::vector<Host>::const_iterator end = priv->destinations.end();
            while (it != end) {
                sendHeader(*it, socket, i_nals);
                for (int i = 0; i < i_nals; ++i) {
                    const int packetSize = nals[i].i_payload; // ### right?
                    const uint8_t* payload = nals[i].p_payload;
                    //printf("nal %d (%d %p)\n", i, packetSize, payload);

                    //stream_frame(avctx, nals[i].p_payload, nals[i].i_payload);
                    socket.send(*it, reinterpret_cast<const char*>(payload), packetSize);
                }
                ++it;
            }
            pthread_mutex_unlock(&priv->mutex);
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
    mPriv->stopped = false;
    mPriv->input = buffer;
    mPriv->width = width;
    mPriv->height = height;
    mPriv->size = size;
    mPriv->outputWidth = mPriv->outputHeight = 0;

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_cond_init(&mPriv->encodeCond, 0);
    pthread_cond_init(&mPriv->doneCond, 0);

    mPriv->encoding = false;
}

Encoder::~Encoder()
{
    pthread_mutex_lock(&mPriv->mutex);
    if (!mPriv->destinations.empty()) {
        pthread_mutex_unlock(&mPriv->mutex);
        deinit();
        pthread_mutex_lock(&mPriv->mutex);
    }
    pthread_mutex_unlock(&mPriv->mutex);

    pthread_cond_destroy(&mPriv->encodeCond);
    pthread_cond_destroy(&mPriv->doneCond);
    pthread_mutex_destroy(&mPriv->mutex);

    delete[] mPriv->sps;
    delete[] mPriv->pps;

    delete mPriv;
}

void Encoder::init()
{
    x264_param_t param;
    memset(&param, '\0', sizeof(x264_param_t));
    x264_param_default_preset(&param, "veryfast", "zerolatency");

    param.b_annexb = 0;
    param.b_repeat_headers = 0;
    param.i_slice_max_size = 1500;

    assert(mPriv->width > mPriv->height);
    double ratio = static_cast<double>(mPriv->width) / static_cast<double>(mPriv->height);

    param.i_threads = 1;
    param.i_width = mPriv->outputWidth;
    param.i_height = static_cast<int>(static_cast<double>(mPriv->outputWidth) / ratio);
    assert(param.i_height <= mPriv->outputHeight);
    mPriv->outputHeight = param.i_height;
    param.i_fps_num = 60;
    param.i_fps_den = 1;
    // Intra refresh:
    param.i_keyint_max = 30;
    param.b_intra_refresh = 1;
    //Rate control:
    param.rc.i_vbv_max_bitrate = 24000;
    param.rc.i_vbv_buffer_size = 400;
    param.rc.i_rc_method = X264_RC_CRF;
    param.rc.f_rf_constant = 19;
    param.rc.f_rf_constant_max = 19;
    x264_param_apply_profile(&param, "high");

    mPriv->encoding = false;
    mPriv->encoder = x264_encoder_open(&param);

    x264_nal_t* p_nal;
    int i_nal;
    mPriv->headerSize = x264_encoder_headers(mPriv->encoder, &p_nal, &i_nal);

    if (i_nal != 3 || p_nal[0].i_type != NAL_SPS || p_nal[1].i_type != NAL_PPS
        || p_nal[0].i_payload < 4 || p_nal[1].i_payload < 1) {
        fprintf(stderr, "Incorrect x264 header!\n");
        exit(0);
    }

#if (X264_BUILD < 127)
# error x264 version too old
#endif

    mPriv->spsSize = p_nal[0].i_payload - 4;
    mPriv->sps = new uint8_t[mPriv->spsSize];
    memcpy(mPriv->sps, p_nal[0].p_payload + 4, mPriv->spsSize);

    mPriv->ppsSize = p_nal[1].i_payload - 4;
    mPriv->pps = new uint8_t[mPriv->ppsSize];
    memcpy(mPriv->pps, p_nal[1].p_payload + 4, mPriv->ppsSize);

    x264_picture_alloc(&mPriv->pic_in, X264_CSP_I420, mPriv->outputWidth, mPriv->outputHeight);

    mPriv->scale = sws_getContext(mPriv->width, mPriv->height, PIX_FMT_BGR24,
                                  mPriv->outputWidth, mPriv->outputHeight,
                                  PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

    pthread_create(&mPriv->thread, 0, EncoderPrivate::run, mPriv);
}

void Encoder::deinit()
{
    pthread_mutex_lock(&mPriv->mutex);
    mPriv->stopped = true;
    pthread_cond_signal(&mPriv->encodeCond);
    pthread_mutex_unlock(&mPriv->mutex);
    void* ret;
    pthread_join(mPriv->thread, &ret);
    mPriv->stopped = false;

    x264_picture_clean(&mPriv->pic_in);
    x264_encoder_close(mPriv->encoder);

    mPriv->spsSize = 0;
    delete[] mPriv->sps;
    mPriv->sps = 0;
    mPriv->ppsSize = 0;
    delete[] mPriv->pps;
    mPriv->pps = 0;
}

void Encoder::ref(const Host& host, int width, int height)
{
    pthread_mutex_lock(&mPriv->mutex);
    if (mPriv->destinations.empty()) {
        mPriv->outputWidth = width;
        mPriv->outputHeight = height;
        pthread_mutex_unlock(&mPriv->mutex);
        init();
        pthread_mutex_lock(&mPriv->mutex);
    }
    mPriv->destinations.push_back(host);
    pthread_mutex_unlock(&mPriv->mutex);
}

void Encoder::deref(const Host& host)
{
    pthread_mutex_lock(&mPriv->mutex);
    std::vector<Host>::iterator it = mPriv->destinations.begin();
    const std::vector<Host>::const_iterator end = mPriv->destinations.end();
    while (it != end) {
        if (it->address() == host.address()
            && it->port() == host.port()) {
            mPriv->destinations.erase(it);
            if (mPriv->destinations.empty()) {
                pthread_mutex_unlock(&mPriv->mutex);
                deinit();
                pthread_mutex_lock(&mPriv->mutex);
            }
            pthread_mutex_unlock(&mPriv->mutex);
            return;
        }
        ++it;
    }
    pthread_mutex_unlock(&mPriv->mutex);
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
    if (mPriv->destinations.empty()) {
        pthread_mutex_unlock(&mPriv->mutex);
        return;
    }
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

int Encoder::encodedWidth() const
{
    return mPriv->outputWidth;
}

int Encoder::encodedHeight() const
{
    return mPriv->outputHeight;
}
