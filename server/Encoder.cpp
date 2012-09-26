#include "Encoder.h"
#include "UdpSocket.h"
#include "Util.h"
#include <vector>
#include <sched.h>
#include <assert.h>
extern "C" {
#include <x264.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

class EncoderPrivate : public Thread
{
public:
    const uint8_t* input;
    int32_t width, height;

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

    Mutex mutex;
    WaitCondition encodeCond, doneCond;
    bool encoding;

    virtual void run();

    int outputWidth, outputHeight;
    std::vector<Host> destinations;
    UdpSocket socket;

    bool stopped;

    uint8_t* nalBuffer;
    int nalBufferSize;
    int nalCount;
};

static void processNals(x264_t *h, x264_nal_t *nal, void *opaque)
{
    EncoderPrivate* priv = static_cast<EncoderPrivate*>(opaque);
    const int targetSize = nal->i_payload*3/2 + 5 + 16;
    if (targetSize > priv->nalBufferSize) {
        delete[] priv->nalBuffer;
        priv->nalBuffer = new uint8_t[targetSize];
        priv->nalBufferSize = targetSize;
    }
    x264_nal_encode(h, priv->nalBuffer, nal);

    MutexLocker locker(&priv->mutex);
    ++priv->nalCount;
    std::vector<Host>::const_iterator it = priv->destinations.begin();
    const std::vector<Host>::const_iterator end = priv->destinations.end();
    while (it != end) {
        const int packetSize = nal->i_payload; // ### right?
        const uint8_t* payload = nal->p_payload;
        //printf("nal %d (%d %p)\n", i, packetSize, payload);

        //stream_frame(avctx, nals[i].p_payload, nals[i].i_payload);
        priv->socket.send(*it, reinterpret_cast<const char*>(payload), packetSize);
        ++it;
    }
}

static inline void sendSeparator(const Host& host, UdpSocket& socket, uint32_t count)
{
    static uint64_t header = 0xbeeffeed00000000LL;
    //printf("sending %u nals\n", nalCount);
    header &= 0xffffffff00000000LL;
    header |= static_cast<uint64_t>(count);
    socket.send(host, reinterpret_cast<char*>(&header), 8);
}

void EncoderPrivate::run()
{
    const int32_t w = width;
    const int32_t h = height;
    int frame = 0;
    for (;;) {
        MutexLocker locker(&mutex);
        while (!encoding) {
            if (stopped)
                return;
            encodeCond.wait(&mutex);
            if (stopped)
                return;
        }
        locker.unlock();

        int srcstride = w * 4; // RGB stride is 3 * width
        sws_scale(scale, &input, &srcstride, 0, h, pic_in.img.plane, pic_in.img.i_stride);

        x264_nal_t* nals;
        int i_nals;
        (void)x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);

        locker.relock();

        if (nalCount) {
            std::vector<Host>::const_iterator it = destinations.begin();
            const std::vector<Host>::const_iterator end = destinations.end();
            while (it != end) {
                sendSeparator(*it, socket, nalCount);
                ++it;
            }
            nalCount = 0;
        }

        encoding = false;
        doneCond.signal();

        locker.unlock();
        yield();
    }
}

Encoder::Encoder(const uint8_t* buffer, int32_t width, int32_t height)
    : mPriv(new EncoderPrivate)
{
    mPriv->stopped = false;
    mPriv->input = buffer;
    mPriv->width = width;
    mPriv->height = height;
    mPriv->outputWidth = mPriv->outputHeight = 0;
    mPriv->nalBuffer = 0;
    mPriv->nalBufferSize = mPriv->nalCount = 0;

    mPriv->encoding = false;
}

Encoder::~Encoder()
{
    MutexLocker locker(&mPriv->mutex);
    if (!mPriv->destinations.empty()) {
        locker.unlock();
        deinit();
        locker.relock();
    }
    locker.unlock();

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
    mPriv->pic_in.opaque = mPriv;

    mPriv->scale = sws_getContext(mPriv->width, mPriv->height, PIX_FMT_RGB32,
                                  mPriv->outputWidth, mPriv->outputHeight,
                                  PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);

    // x264_encoder_headers() crashes when nalu_process is set
    // additionally, x264_encoder_reconfigure doesn't respect nalu_process
    // so a close and reopen is required here
    param.nalu_process = processNals;
    x264_encoder_close(mPriv->encoder);
    mPriv->encoder = x264_encoder_open(&param);

    mPriv->start();
}

void Encoder::deinit()
{
    MutexLocker locker(&mPriv->mutex);
    mPriv->stopped = true;
    mPriv->encodeCond.signal();
    locker.unlock();
    void* ret;
    mPriv->join();
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
    MutexLocker locker(&mPriv->mutex);
    if (mPriv->destinations.empty()) {
        mPriv->outputWidth = width;
        mPriv->outputHeight = height;
        locker.unlock();
        init();
        locker.relock();
    }
    mPriv->destinations.push_back(host);
}

void Encoder::deref(const Host& host)
{
    MutexLocker locker(&mPriv->mutex);
    std::vector<Host>::iterator it = mPriv->destinations.begin();
    const std::vector<Host>::const_iterator end = mPriv->destinations.end();
    while (it != end) {
        if (it->address() == host.address()
            && it->port() == host.port()) {
            mPriv->destinations.erase(it);
            if (mPriv->destinations.empty()) {
                locker.unlock();
                deinit();
                locker.relock();
            }
            return;
        }
        ++it;
    }
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
    MutexLocker locker(&mPriv->mutex);
    if (mPriv->destinations.empty())
        return;
    while (mPriv->encoding) {
        mPriv->doneCond.wait(&mPriv->mutex);
    }
    mPriv->encoding = true;
    mPriv->encodeCond.signal();
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
