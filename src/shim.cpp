#include <MinHook.h>

#include <gl/gl.h>
#include <windows.h>

#include <chrono>
#include <cinttypes>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

typedef BOOL(WINAPI* SwapBuffersType)(HDC hdc);

SwapBuffersType OriginalSwapBuffers = nullptr;

static bool saved{};
static DWORD error{};
static void* buffer{};
static HANDLE event{};
static HANDLE mapFile{};
static std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> start;

BOOL WINAPI DetourSwapBuffers(HDC hdc) {
    HGLRC context = wglGetCurrentContext();
    if (context) {
        int viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int width  = viewport[2];
        int height = viewport[3];
        //std::vector<uint8_t> pixels(3 * width * height);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

        auto now  = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (!saved && time.count() >= 5.0) {
            //std::stringstream data;

            auto s = std::chrono::high_resolution_clock::now();
            //for (int i = 0; i < height; i++) {
            //    for (int j = 0; j < width; j++) {
            //        int index      = (i * width + j) * 3;
            //        uint8_t* pixel = &pixels[index];
            //        data << pixel[0] << pixel[1] << pixel[2];
            //    }
            //}

            //const size_t bufferSize = 1024 * 1024 * 12;

            //auto dataString  = data.str();
            //char* dataBuffer = dataString.data();
            //size_t length    = dataString.length();
            //memcpy(buffer, &length, sizeof(length));
            //memcpy((char*) buffer + 8, dataBuffer, dataString.length());
            SetEvent(event);

            auto e = std::chrono::high_resolution_clock::now();

            std::cerr << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count()
                      << '\n';
            saved = true;
        }
    } else {
        MessageBoxA(nullptr, "Could not load context\n", "Message", MB_OK);
    }
    return OriginalSwapBuffers(hdc);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        MessageBox(nullptr, "Message", "Message", MB_OK);
        start = std::chrono::high_resolution_clock::now();

        mapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "Local\\VideoFrame");
        if (!mapFile) {
            std::cerr << "Error opening memory mapped file\n";
            return 1;
        }

        buffer = MapViewOfFile(mapFile, FILE_MAP_ALL_ACCESS, 0, 0, 1024 * 1024 * 12);

        event = OpenEvent(EVENT_MODIFY_STATE, FALSE, "Local\\VideoFrameReady");
        if (!event) {
            std::cerr << "Error opening frame ready event\n";
            return 1;
        }

        if (MH_Initialize() != MH_OK) {
            std::cerr << "Could not init minhook\n";
            return 1;
        }

        if (MH_CreateHookApi(L"gdi32",
                             "SwapBuffers",
                             (void*) &DetourSwapBuffers,
                             reinterpret_cast<LPVOID*>(&OriginalSwapBuffers))) {
            std::cerr << "Could not create hook for swapBuffers\n";
            return 1;
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            std::cerr << "Could not enable hook\n";
            return 1;
        }

        break;
    case DLL_PROCESS_DETACH:
        if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
            return 1;
        }

        if (MH_Uninitialize() != MH_OK) {
            return 1;
        }

        CloseHandle(mapFile);
        break;
    }
    return TRUE;
}
