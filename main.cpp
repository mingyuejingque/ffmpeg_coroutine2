#include <stdio.h>
#include <functional>
#include "muxing.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("usage: %s <input_file_name>\n", argv[0]);
        return 0;
    }
    const char* input_file_name = argv[1];
    FILE *fp = fopen(input_file_name, "rb");
    if (!fp) {
        printf("can not open file[%s]\n", input_file_name);
        return 0;
    }

    ff_context *ctx = nullptr;
    init_ffmpeg(&ctx);
    if (!ctx) {
        printf("init ffmpeg faild\n");
        return 0;
    }

    int len = 0;
    static char buffer[4096];
    my_push_type push(std::bind(resume_ffmpeg, std::placeholders::_1, ctx));
    while(1) {
        //do_some_thing_
        printf("\nmain.loop begin\n");
        
        { //get_some_media_data;
            len = fread(buffer, 1, sizeof(buffer), fp);
            if (len == 0) {
                stop_ffmpeg();
                break;
            }
        }

        fill_data_to_ffmpeg(ctx, (uint8_t*)buffer, len);
        if (!push) {
            push = std::move(my_push_type(std::bind(resume_ffmpeg, std::placeholders::_1, ctx)));
        }
        push();

        //do_some_thing
        printf("main.loop end\n");
    }

    uninit_ffmpeg(ctx);
    ctx = nullptr;
    fclose(fp);
    return 0;
}
