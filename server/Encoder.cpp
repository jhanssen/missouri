#include "Encoder.h"
#include <pthread.h>
#include <sched.h>
extern "C" {
#include <x264.h>
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

	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t encodeCond, doneCond;
	bool encoding;

	static void* run(void* arg);
};

void* EncoderPrivate::run(void* arg)
{
	EncoderPrivate* priv = static_cast<EncoderPrivate*>(arg);
	for (;;) {
		pthread_mutex_lock(&priv->mutex);

		while (!priv->encoding) {
			pthread_cond_wait(&priv->encodeCond, &priv->mutex);
		}
		pthread_mutex_unlock(&priv->mutex);

		// do the actual encoding here

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

	// Intra refres:
	param.i_keyint_max = 60;
	param.b_intra_refresh = 1;

	// Rate control:
	param.rc.i_rc_method = X264_RC_CRF;
	param.rc.f_rf_constant = 25;
	param.rc.f_rf_constant_max = 35;

	// For streaming:
	param.b_repeat_headers = 1;
	param.b_annexb = 1;
	x264_param_apply_profile(&param, "baseline");

	mPriv->encoder = x264_encoder_open(&param);
	x264_picture_alloc(&mPriv->pic_in, X264_CSP_I420, width, height);

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
