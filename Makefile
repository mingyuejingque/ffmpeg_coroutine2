
ffmpeg_rtp: muxing.cpp main.cpp
	clang++ -g -std=c++11 -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
		-o ffmpeg_rtp main.cpp muxing.cpp \
		-lboost_context -lboost_thread -lboost_system \
		-lavformat -lavcodec -lavdevice -lswresample -lswscale  -lavfilter -lpostproc -lavutil  \
		-lz -llzma -lbz2 -lvpx -lx265 -lx264  \
		-lfdk-aac -lmp3lame \
		-lpthread -liconv -ldl\
		-L/home/delphi/software/boost/boost_1_66_0_clang/stage/lib \
		-I/home/delphi/software/boost/boost_1_66_0_clang/stage/include \

