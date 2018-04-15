/**
 * @file
 * 
 *
 * Remux streams from one container format to another.
 */

#include <functional>

#include <sys/time.h>
#include <unistd.h>
#include "muxing.h"

static const int g_fifo_len = 4 * 1024 * 1024;
static const int g_avio_buffer_len = 64 * 1024;
static bool g_exit = false;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag);
static bool open_input_file(ff_context* ctx);
static bool open_output_file(ff_context* ctx);
static bool handle_frame_data(ff_context* ctx);

struct ff_context {
    AVFifoBuffer* fifo;
    AVIOContext *avio_ctx;
    uint8_t* avio_buffer;
    int avio_buffer_len;
    AVOutputFormat *ofmt;
    AVInputFormat *ifmt;
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    AVPacket pkt;

    bool has_probe_input;
    bool has_output_file;
    bool has_probe_output;
    char output_file_name[256];
    char output_format[256];

    int video_stream_index;

    my_push_type* push;
    my_pull_type* pull;

    ff_context() {
        has_probe_input = false;
        has_probe_output = false;
        has_output_file = true; // 设置为true表示需要输出的avformatcontext， (false else)
        video_stream_index = -1;

        fifo = av_fifo_alloc(g_fifo_len);
        push = nullptr;
        pull = nullptr;
    }
};

void write_to_file(uint8_t* data, int len) {
    static FILE* fp = fopen("out2.mp4", "ab");
    fwrite(data, 1, len, fp);
}

void fill_data_to_ffmpeg(ff_context* ctx, uint8_t* data, int size) {
    if (data && size) {
        int space = av_fifo_space(ctx->fifo);
        if (space < size) {
            fprintf(stderr, "fifo space:%d < size:%d !!!!!", space, size);
        }
        int will_write = FFMIN(size, space);
        av_fifo_generic_write(ctx->fifo, data, will_write, nullptr);
        fprintf(stderr, "%s write 0x%x bytes to fifo\n", __func__, will_write);
    }
}

void resume_ffmpeg(my_pull_type &pull, ff_context* ctx) {
    fprintf(stderr, "%s called.\n", __func__);
    ctx->pull = &pull;
    handle_frame_data(ctx);
}


int avio_read_packet(void* opaque, uint8_t* buf, int size) {
    ff_context* ctx = (ff_context*)opaque;
    int will_read = FFMIN(size, av_fifo_size(ctx->fifo));
    while (!will_read && !g_exit) {
        fprintf(stderr, "%s yield\n", __func__);
        (*ctx->pull)();
        will_read = FFMIN(size, av_fifo_size(ctx->fifo));
    }

    if (will_read == 0)
        return AVERROR_EOF;
    av_fifo_generic_read(ctx->fifo, buf, will_read, nullptr);
    fprintf(stderr, "%s read: 0x%x\n", __func__, will_read);
    return will_read;
}

bool init_ffmpeg(ff_context** context, const char* output_file, const char* output_format) {
    bool ret = false;
    ff_context *ctx = new ff_context();
    *context = ctx;
    strncpy(ctx->output_file_name, output_file, strlen(output_file));
    strncpy(ctx->output_format, output_format, strlen(output_format));

    av_register_all();
    avformat_network_init();

    ctx->avio_buffer_len = g_avio_buffer_len;
    ctx->avio_buffer = (uint8_t*)av_malloc(ctx->avio_buffer_len);
    ctx->avio_ctx = avio_alloc_context(ctx->avio_buffer, 
                                        ctx->avio_buffer_len,
                                        0,
                                        ctx,
                                        avio_read_packet,
                                        nullptr,
                                        nullptr);

    ret = (ctx->avio_buffer != nullptr) && (ctx->avio_ctx != nullptr);
    if (!ret) 
        return ret;

    return ret;
}

bool uninit_ffmpeg(ff_context* context) {
    fprintf(stderr, "%s called.\n", __func__);
    ff_context *ctx = (ff_context*)context;
    if (!ctx) {
        return true;
    }

    if (ctx->has_probe_output && ctx->ofmt_ctx) {
        av_write_trailer(ctx->ofmt_ctx);
        fprintf(stderr, "av_write_trailer.\n");
        if (ctx->ofmt_ctx && !(ctx->ofmt->flags & AVFMT_NOFILE)) {
            avio_closep(&ctx->ofmt_ctx->pb);
            avformat_free_context(ctx->ofmt_ctx);
        }
    }
    if (ctx->has_probe_input) {
        avformat_close_input(&ctx->ifmt_ctx);
    }
    av_freep(&ctx->avio_ctx->buffer); //<== ctx->avio_buffer
    av_freep(&ctx->avio_ctx);
    av_fifo_free(ctx->fifo);
    delete ctx;
    fprintf(stderr, "%s finish.\n", __func__);
    return true;
}

bool stop_ffmpeg() {
    fprintf(stderr, "%s called.\n", __func__);
    g_exit = true;
    return true;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
#if 1
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    fprintf(stderr, "%s: [timebase %d/%d] pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           tag,
           time_base->num, time_base->den,
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
#endif
}

static bool open_input_file(struct ff_context* ctx) {
    int ret = 0;
    ctx->ifmt_ctx = avformat_alloc_context();
    ctx->ifmt_ctx->pb = ctx->avio_ctx;
    ctx->ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;

    if ((ret = avformat_open_input(&ctx->ifmt_ctx, NULL, NULL, NULL)) < 0) {
        fprintf(stderr, "avformat_open_input error: %d\n", ret);
        return ret;
    }

    if ((ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL)) < 0) {
        fprintf(stderr, "find stream info err: %d\n", ret);
        return ret;
    }

    av_dump_format(ctx->ifmt_ctx, 0, nullptr, 0);
    return true;
}

static bool open_output_file(struct ff_context* ctx) {
    int ret = 0;
    if (ctx->has_output_file && strlen(ctx->output_file_name) > 1) {
        avformat_alloc_output_context2(
                &ctx->ofmt_ctx,
                NULL,
                ctx->output_format,
                ctx->output_file_name
                );
        if (!ctx->ofmt_ctx) {
            fprintf(stderr, "could not create output!\n");
            return ret;
        }

        ctx->ofmt = ctx->ofmt_ctx->oformat;
        AVCodec* codec;
        int video_stream_index = av_find_best_stream(ctx->ifmt_ctx, 
                                                    AVMEDIA_TYPE_VIDEO,
                                                    -1,
                                                    -1,
                                                    &codec,
                                                    0);

        if (video_stream_index == -1) {
            return ret;
        }

        ctx->video_stream_index = video_stream_index;
        AVStream *out_stream;
        AVStream *in_stream = ctx->ifmt_ctx->streams[video_stream_index];
        AVCodecParameters *in_codepar = in_stream->codecpar;

        fprintf(stderr, "found video stream, index: %d\n", video_stream_index);
        out_stream = avformat_new_stream(ctx->ofmt_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "faild allocate output stream.\n");
            return ret;
        }
        avcodec_parameters_copy(out_stream->codecpar, in_codepar);
        out_stream->codecpar->codec_tag = 0;

        av_dump_format(ctx->ofmt_ctx, 0, ctx->output_file_name, 1);
        if (!(ctx->ofmt_ctx->flags & AVFMT_NOFILE)) {
            ret = avio_open(&ctx->ofmt_ctx->pb, ctx->output_file_name, AVIO_FLAG_WRITE);
            if (ret < 0) {
                fprintf(stderr, "could not open file.\n");
                return false;
            }
        }
        ret = avformat_write_header(ctx->ofmt_ctx, NULL);
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file\n");
            return false;
        }
    }

    return true;
}


static bool handle_frame_data(ff_context* ctx) {
    fprintf(stderr, "%s called.\n", __func__);
    int ret = 0;
    if (!ctx->has_probe_input) {
        ret = open_input_file(ctx);
        fprintf(stderr, "open_input_file: %d\n", ret);
        if (!ret) 
            return ret;
        ctx->has_probe_input = true;
    }

    if (ctx->has_output_file && !ctx->has_probe_output) {
        ret = open_output_file(ctx);
        fprintf(stderr, "open_output_file: %d\n", ret);
        if (!ret)
            return ret;
        ctx->has_probe_output = true;
    }

    if (ctx->has_probe_input) {
        AVStream *in_stream, *out_stream;
        while (1) {
            ret = av_read_frame(ctx->ifmt_ctx, &ctx->pkt);
            if (ret < 0) {
                fprintf(stderr, "%s av_read_frame ret: %d\n",
                        __func__, ret);
                break;
            }

            //filter some data (audio etc.)
            if (ctx->pkt.stream_index != ctx->video_stream_index) {
                av_packet_unref(&ctx->pkt);
                continue;
            }

            in_stream = ctx->ifmt_ctx->streams[ctx->pkt.stream_index];
            out_stream = ctx->ofmt_ctx->streams[0];
            ctx->pkt.stream_index = 0;
            log_packet(ctx->ifmt_ctx, &ctx->pkt, "in");
            
            /*copy packet.*/
            ctx->pkt.pts = av_rescale_q_rnd(ctx->pkt.pts, 
                                            in_stream->time_base,
                                            out_stream->time_base,
                                            AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            ctx->pkt.dts = av_rescale_q_rnd(ctx->pkt.dts,
                                            in_stream->time_base,
                                            out_stream->time_base,
                                            AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            ctx->pkt.duration = av_rescale_q(ctx->pkt.duration, in_stream->time_base, out_stream->time_base);
            ctx->pkt.pos = -1;
            log_packet(ctx->ofmt_ctx, &ctx->pkt, "  out");

            if (ctx->has_probe_output) {
                ret = av_interleaved_write_frame(ctx->ofmt_ctx, &ctx->pkt);
                if (ret < 0) {
                    fprintf(stderr, "Error muxing packet: %s\n", strerror(ret));
                    break;
                }
            }
            av_packet_unref(&ctx->pkt);
        }
    }
    return ret;
}
