#ifndef _MY_DZ_H_
#define _MY_DZ_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif 
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/fifo.h>
#ifdef __cplusplus
}
#endif 


#include <boost/coroutine2/all.hpp>

typedef boost::coroutines2::coroutine<void>::push_type my_push_type;
typedef boost::coroutines2::coroutine<void>::pull_type my_pull_type;

struct ff_context;

bool init_ffmpeg(ff_context** context);
bool stop_ffmpeg();
bool uninit_ffmpeg(ff_context* context);

void fill_data_to_ffmpeg(ff_context* ctx, uint8_t* data, int size);
void resume_ffmpeg(my_pull_type &pull, ff_context* ctx);

#endif
