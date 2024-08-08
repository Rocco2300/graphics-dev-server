#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

#include <windows.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

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

    std::ofstream out("C:/Users/grigo/Desktop/test.ppm");
    DWORD result = WaitForSingleObject(event, INFINITE);
    if (result == WAIT_OBJECT_0) {
        auto s = std::chrono::high_resolution_clock::now();
        auto* data = (uint8_t*) buffer;
        //out.write((char*)buffer + 8, *(size_t*)buffer);
        out << "P3\n";
        out << 1280 << ' ' << 720 << '\n';
        out << "255\n";
        for (int i = 0; i < 720; i++) {
            for (int j = 0; j < 1280; j++) {
                int index      = (i * 1280 + j) * 3;
                uint8_t* pixel = &data[index];
                out << static_cast<int>(pixel[0]) << ' ' << static_cast<int>(pixel[1]) << ' '
                    << static_cast<int>(pixel[2]) << '\n';
            }
        }
        auto e = std::chrono::high_resolution_clock::now();

        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count()
                  << '\n';
        //out.write((char*)buffer, 1024 * 1024 * 12);
    }

    ResetEvent(event);

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(threadHandle);

    return 0;
}