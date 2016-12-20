#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <stdio.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/syscall.h>
#include <signal.h>
#include <time.h>

#include <iostream>
#include <fstream>
#include <time.h>

//#include <mutex>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#ifdef __cplusplus
};
#endif
#endif

using namespace std;
unsigned char bufferFifo[327680] = {0};
int buffLen = 0;
int head = 0;
int end = 0;
unsigned char testData[200000] = {0};

bool INeedExit = false;
bool INeedPause = false;
const char *in_fileName;
//std::mutex mtx;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void *bufferThread(void *nothing);
void *readStdinThread(void *nothing);
void pauseOrStart(void);
int read_packet(void *bufferFifo, unsigned char *bufferOut, int buf_size);

void exitHandler(int p) {
    INeedExit = true;
    printf("\nreceive SIGTERM\n\n");
}

#define BLOCKWHENPAUSE 1

int main(int argc, char* argv[]) {
    signal(SIGTERM, exitHandler);
    signal(SIGINT, exitHandler);
/*
        ofstream ofs;
        ofs.open("log.log", ios::out|ios::app|ios::trunc);
        
        int OutPutFd = open("ffmTest.output", O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
        if (-1 == OutPutFd){
            ofs << "open ffmTest.output fail"<<endl;\
        ofs.close();
            return 0;
        }
        if (-1 == dup2(OutPutFd, STDOUT_FILENO)){
            ofs << "dup2 fail"<<endl;
            ofs.close();
            close(OutPutFd);
            return 0;
        }
 */ 
    time_t rawTime;
    //struct tm *timeInfo;
    time(&rawTime);
    printf("%sffmTest started.\n\n", ctime(&rawTime));

    pthread_t thread;
    pthread_t thread2;
    char* out_filename = argv[1];
    //char* out_filename = "rtsp://120.27.188.84:8888/realtime/$1234/1/realtime.sdp";
    if (argc != 2) {
        printf("usage: ./ffmTest rtsp://...sdp");
        return 0;
    }
    printf("argv[1]: %s\n\n", argv[1]);

    AVOutputFormat *ofmt = NULL;
    //输入对应一个AVFormatContext，输出对应一个AVFormatContext
    //（Input AVFormatContext and Output AVFormatContext）
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVPacket pkt;
    //const char *in_filename1;
    int ret;
    unsigned int i;
    int videoindex = -1;
    int frame_index = 0;
    int64_t start_time = 0;

    // run path is /mnt/hgfs/ShareFolder/ffmTest/out/JavaSub/
    in_fileName = "/mnt/hgfs/ShareFolder/ffmTest/out/JavaSub/video.h264";

    pthread_create(&thread, NULL, bufferThread, NULL);
    pthread_create(&thread2, NULL, readStdinThread, NULL);

    usleep(100000);
    unsigned char * iobuffer = (unsigned char *) av_malloc(327680);
    //in_filename1 = "/home/public/sun_jianfeng/ffmTest/out/video.mp4";

    av_register_all();
    //Network
    avformat_network_init();

    ifmt_ctx = avformat_alloc_context();
    AVIOContext *avio = avio_alloc_context(iobuffer, 327680, 0, bufferFifo, read_packet, NULL, NULL);
    ifmt_ctx->pb = avio;
    ifmt_ctx->probesize = 100 * 1024;
    //ifmt_ctx->max_analyze_duration = 5 * AV_TIME_BASE;

    //输入（Input）
    if ((ret = avformat_open_input(&ifmt_ctx, in_fileName, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }

    //输出（Output）

    avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", out_filename); //RTsP

    if (!ofmt_ctx) {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //根据输入流创建输出流（Create output AVStream according to input AVStream）
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        if (!out_stream) {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        //复制AVCodecContext的设置（Copy the settings of AVCodecContext）
        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            printf("Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    //Dump Format------------------
    av_dump_format(ofmt_ctx, frame_index, out_filename, 1);

    //打开输出URL（Open output URL）
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        AVDictionary *options = NULL;
        av_dict_set(&options, "s", "320x300", 0);
        //ret = avio_open2(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE,NULL,&options);
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("Could not open output URL '%s'", out_filename);
            goto end;
        }
    }

    //写文件头（Write file header）
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        printf("Error occurred when opening output URL\n");
        goto end;
    }

    start_time = av_gettime();

    while (!INeedExit) {
#if BLOCKWHENPAUSE
        pauseOrStart();
#else        
        if (INeedPause){
            sleep(5);
            //continue;
        }
#endif        
        int times;

        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);

        if (ret < 0) {
            printf("av read frame error! %d\n;", errno);
            sleep(1);
            continue;
        }

        if (pkt.pts == AV_NOPTS_VALUE) {
            //Write PTS
            AVRational time_base1 = ifmt_ctx->streams[videoindex]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
            //Parameters
            pkt.pts = (double) (frame_index * calc_duration) / (double) (av_q2d(time_base1) * AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
        }

        //Important:Delay
        if (pkt.stream_index == videoindex) {
            AVRational time_base = ifmt_ctx->streams[videoindex]->time_base;
            AVRational time_base_q = {1, AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

        }

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        /* copy packet */
        //转换PTS/DTS（Convert PTS/DTS）
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        //Print to Screen
        //printf("pkt index:%d\n",videoindex);
        if (pkt.stream_index == videoindex) {
            //printf("Send %8d video frames to output URL  size:%d  \n", frame_index, pkt.size);
            frame_index++;
        }
        times = av_gettime();
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            printf("Error muxing packet\n");
            break;
        }

        av_free_packet(&pkt);
        times++;
    }
    //写文件尾（Write file trailer）
    av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }

    printf("ffmTest done.\n\n");

    return 0;
}

void *bufferThread(void *nothing) {
    int fileFd1 = 0;
    int fileFd2 = 0;

    static int times = 0;
//    static int flag = 0;
    fileFd1 = open(in_fileName, O_RDONLY);

    if (fileFd1 == -1 || fileFd2 == -1) {
        printf("open file filed !!!\n");
        return NULL;
    }

    printf("begin to read data !!!\n");
    while (!INeedExit) {
        // pauseOrStart();
        
        if (buffLen == 0) {
            if (times >= 10000) {
                times = 0;
            }
            if ((buffLen = read(fileFd1, bufferFifo, 327680)) == 0) {
                printf("file end!\n");
                close(fileFd1);
                fileFd1 = open(in_fileName, O_RDONLY);
            }
            //printf("read data1! buffLen:%d\n",buffLen);
            times++;
        }
        usleep(100);
    }
    return NULL;
}

void *readStdinThread(void* nothing) {

    //bool isPushing = true;
    string cmd;
    for (; !INeedExit;) {
        cin >> cmd;
        if (cmd[0] == 'p' && !INeedPause) {
            //printf("read p.\n");
            INeedPause = true;
#if BLOCKWHENPAUSE
            pthread_mutex_lock(&mutex);
#endif            
            printf("readStdinThread locked.\n");
        } else if (cmd[0] == 'c' && INeedPause) {
            //printf("read c.\n");
            INeedPause = false;
#if BLOCKWHENPAUSE
            pthread_mutex_unlock(&mutex);
#endif
            printf("readStdinThread unlocked.\n");
        }
    }
    return NULL;
}

int read_packet(void *bufferFifo, unsigned char *bufferOut, int buf_size) {
    //printf("get data buf_size:%d   buffLen:%d!\n",buf_size,buffLen);

    static int times = 0;

    if (times % 60 == 30) {

    }
    //printf("the id of read_packet thread:%ld\n", syscall(SYS_gettid));
    //printf("buffLen end!:%d  times:%d\n", buffLen, times);

    times++;

    if (buf_size >= 0 && buf_size <= buffLen) {

        memcpy(bufferOut, bufferFifo, buf_size);

        buffLen = 0;
        usleep(10000);

        return buf_size;
    } else if (buf_size >= 0 && buf_size > buffLen) {
        buf_size = buffLen;
        memcpy(bufferOut, bufferFifo, buf_size);

        buffLen = 0;
        usleep(10000);
        return buf_size;
    }
    return 0;
}

void pauseOrStart(void) {
    if (INeedPause) {
        // bolck here until readStdinThread read c and unlock
        printf("\n");
        time_t t = time(NULL);
        struct tm* current_time = localtime(&t);
        printf("ffmpeg pause. %d:%d:%d\n",
                current_time->tm_hour,
                current_time->tm_min,
                current_time->tm_sec);
        //mtx.lock();
        pthread_mutex_lock(&mutex);

        //mtx.unlock();
        pthread_mutex_unlock(&mutex);
        //t = time(NULL);
        t = time(NULL);
        current_time = localtime(&t);
        printf("ffmpeg start. %d:%d:%d\n",
                current_time->tm_hour,
                current_time->tm_min,
                current_time->tm_sec);
    }
}

