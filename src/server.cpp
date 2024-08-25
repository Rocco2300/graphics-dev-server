#include <iostream>
#include <string>

#include <boost/asio.hpp>

#include <windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
};

namespace asio = boost::asio;
using asio::ip::udp;

static int frameNumber{};

static constexpr char dllName[] = "C:/Users/grigo/Repos/graphics-dev-server/bin/libshim.dll";
static constexpr unsigned int dllNameLen = sizeof(dllName) + 1;

int APIENTRY WinMain(HINSTANCE inst, HINSTANCE prevInst, PSTR cmdline, int cmdshow) {
    HMODULE kernelHandle = GetModuleHandleA("Kernel32");
    VOID* libraryHandle  = (VOID*) GetProcAddress(kernelHandle, "LoadLibraryA");

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string program   = cmdline;
    std::string directory = program.substr(0, program.find_last_of('/'));

    if (!CreateProcessA(program.c_str(),
                        nullptr,
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        directory.c_str(),
                        &si,
                        &pi)) {
        std::cerr << "Failed to launch the program " << program << '\n';
        return 1;
    }

    HANDLE mapFile = CreateFileMapping(INVALID_HANDLE_VALUE,
                                       nullptr,
                                       PAGE_READWRITE,
                                       0,
                                       1024 * 1024 * 12,
                                       "Local\\VideoFrame");
    if (!mapFile) {
        std::cerr << "Could not create file mapping for the video frame\n";
        std::cerr << GetLastError() << '\n';
        return 1;
    }
    auto buffer = MapViewOfFile(mapFile, FILE_MAP_ALL_ACCESS, 0, 0, 1024 * 1024 * 12);
    if (!buffer) {
        std::cerr << "Error allocating buffer\n";
        return 1;
    }

    LPVOID remoteBuffer = VirtualAllocEx(pi.hProcess,
                                         nullptr,
                                         dllNameLen,
                                         MEM_RESERVE | MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    if (!remoteBuffer) {
        std::cerr << "Error allocating memory in remote process\n";
        return 1;
    }

    if (!WriteProcessMemory(pi.hProcess, remoteBuffer, dllName, dllNameLen, nullptr)) {
        std::cerr << "Could not write memory into remote process\n";
        return 1;
    }

    auto threadHandle = CreateRemoteThread(pi.hProcess,
                                           nullptr,
                                           0,
                                           (LPTHREAD_START_ROUTINE) libraryHandle,
                                           remoteBuffer,
                                           0,
                                           nullptr);
    if (!threadHandle) {
        std::cerr << "Could not launch remote thread\n";
        return 1;
    }

    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, "Local\\VideoFrameReady");
    if (!event) {
        std::cerr << "Failed to create event\n";
        return 1;
    }

    HANDLE mutex = CreateMutex(nullptr, FALSE, "Local\\VideoFrameMutex");
    if (!mutex) {
        std::cerr << "Error creating mutex for video frame\n";
        return 1;
    }

    auto codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "Cannot find h264 codec\n";
        return 1;
    }

    auto context = avcodec_alloc_context3(codec);
    if (!context) {
        std::cerr << "Cannot allocate video encoder context.\n";
        return 1;
    }

    context->bit_rate      = 400000;
    context->width         = 1280;
    context->height        = 720;
    context->time_base.num = 1;
    context->time_base.den = 60;
    context->framerate.num = 60;
    context->framerate.den = 1;
    context->pix_fmt       = AV_PIX_FMT_YUV420P;
    context->gop_size      = 2 * 60;
    context->max_b_frames  = 0;

    av_opt_set(context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(context->priv_data, "crf", "25", 0);
    av_opt_set(context->priv_data, "tune", "zerolatency", 0);

    auto desc = av_pix_fmt_desc_get(AV_PIX_FMT_BGRA);
    if (!desc) {
        std::cerr << "Cannot get pixel format description\n";
        return 1;
    }

    auto bytesPerPixel = av_get_bits_per_pixel(desc) / 8;
    if (avcodec_open2(context, codec, nullptr) < 0) {
        std::cerr << "Could not open codec\n";
        return 1;
    }

    auto swsContext = sws_getContext(1280,
                                     720,
                                     AV_PIX_FMT_BGRA,
                                     1280,
                                     720,
                                     AV_PIX_FMT_YUV420P,
                                     0,
                                     nullptr,
                                     nullptr,
                                     nullptr);
    if (!swsContext) {
        std::cerr << "Cannot allocate sws context\n";
        return 1;
    }

    AVFrame* frame   = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    if (!frame) {
        std::cerr << "Could not allocate frame\n";
        return 1;
    }

    frame->width  = context->width;
    frame->height = context->height;
    frame->format = context->pix_fmt;
    if (av_frame_get_buffer(frame, 0) < 0) {
        std::cerr << "Cannot allocate the video frame data\n";
        return 1;
    }

    AVFormatContext* formatContext;
    const AVOutputFormat* fmt = av_guess_format("rtp", nullptr, nullptr);
    if (!fmt) {
        std::cerr << "The rtp format was not found\n";
        return 1;
    }

    if (avformat_alloc_output_context2(&formatContext, fmt, fmt->name, "rtp://127.0.0.1:8000")) {
        std::cerr << "Could not allocate output context for rtp stream\n";
        return 1;
    }

    if (avio_open(&formatContext->pb, formatContext->url, AVIO_FLAG_WRITE)) {
        std::cerr << "Could not open rtp stream\n";
        return 1;
    }

    AVStream* stream             = avformat_new_stream(formatContext, codec);
    //stream->codecpar->bit_rate   = 400000;
    //stream->codecpar->width      = 1280;
    //stream->codecpar->height     = 720;
    //stream->codecpar->codec_id   = AV_CODEC_ID_H264;
    //stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    //stream->time_base.num        = 1;
    //stream->time_base.den        = 60;

    avcodec_parameters_from_context(stream->codecpar, context);

    char buf[20000];
    AVFormatContext* ac[] = {formatContext};
    av_sdp_create(ac, 1, buf, 20000);

    FILE* out = fopen("test.sdp", "w");
    fprintf(out, "%s", buf);
    fclose(out);

    //FILE* debugOut = fopen("test.h264", "wb");
    //debugOut       = fopen("test.h264", "wb");
    while (true) {
        auto eventWait = WaitForSingleObject(event, INFINITE);
        auto mutexWait = WaitForSingleObject(mutex, INFINITE);

        if (eventWait == WAIT_OBJECT_0 && mutexWait == WAIT_OBJECT_0) {
            int ret;
            fflush(stdout);

            auto* data               = (uint8_t*) buffer;
            const uint8_t* inData[1] = {data};
            int inLineSize[1]        = {bytesPerPixel * context->width};
            if (av_frame_make_writable(frame) < 0) {
                std::cerr << "Frame not writtable\n";
            }

            sws_scale(swsContext,
                      inData,
                      inLineSize,
                      0,
                      context->height,
                      frame->data,
                      frame->linesize);
            frame->pts = frameNumber;

            ret = avcodec_send_frame(context, frame);
            if (ret < 0) {
                std::cerr << "Something went wrong when sending frame to encoder\n";
                return 1;
            }
            //frameNumber = (frameNumber + 1) % context->framerate.num;
            frameNumber++;

            while (ret >= 0) {
                ret = avcodec_receive_packet(context, packet);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Something went wrong when receiving packet from encoder\n";
                    return 1;
                }

                auto ret_val = avformat_write_header(formatContext, nullptr);
                int64_t frameTime{};
                int64_t frameDuration{};

                frameDuration    = stream->time_base.den / 60;
                frameTime        = frameNumber * frameDuration;
                packet->pts      = frameTime / stream->time_base.num;
                packet->duration = frameDuration;

                packet->dts          = packet->pts;
                packet->stream_index = stream->index;

                //std::cout << packet->pts << ' ' << packet->dts << '\n';
                //fwrite(packet->data, packet->size, 1, debugOut);

                //packet->pts = av_rescale_q(packet->pts, context->time_base, stream->time_base);
                //packet->dts = av_rescale_q(packet->dts, context->time_base, stream->time_base);
                av_interleaved_write_frame(formatContext, packet);
                av_packet_unref(packet);
            }

            ReleaseMutex(mutex);
            ResetEvent(event);
        }
    }

    avcodec_free_context(&context);
    sws_freeContext(swsContext);
    av_frame_free(&frame);

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(threadHandle);

    return 0;
}