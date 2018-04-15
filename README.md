# ffmpeg_coroutine2
ffmpeg custom IO using boost.coroutine2

一般来说，ffmpeg中的 read_func 回调算是阻塞函数， 要准时填充数据给它才行， 要是直接返回 0 或者 AVERROR_EOF ffmpeg就会认为失败或者文件结束了。
为了实现让ffmpeg 从内存、网络、各种sdk里读数据， 需要自定义 custom IO 回调。 参考 ffmpeg/doc/example/avio_reading.c 

一种玩法：
可以启动 2 个线程， 一个专门用来获取数据 且 填充到缓存队列；  另一个线程专门为了ffmpeg的循环（就算read_func回调阻塞也不影响其他线程）


另一种玩法：
使用 coroutine 在一个线程里实现了类似 “生产者-消费者”模型， 当 ffmpeg. read_func 发现没有数据可读的时候就立刻 切换 出去，
让其他工作有机会继续进行(比如接收网络数据，读设备sdk等等）； 以后在适当时机又切换回到 read_func中上次的地方继续运行。

本例子里用了boost.coroutine2
