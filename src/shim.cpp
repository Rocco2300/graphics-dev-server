#include <MinHook.h>

#include <gl/gl.h>
#include <windows.h>

#include <chrono>
#include <iostream>
#include <thread>

typedef BOOL(WINAPI* SwapBuffersType)(HDC hdc);

SwapBuffersType OriginalSwapBuffers = nullptr;

static bool saved{};
static DWORD error{};
static void* buffer{};
static HANDLE event{};
static HANDLE mutex{};
static HANDLE mapFile{};
static std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> start;
static std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> frameEnd;
static std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> currTime;

BOOL WINAPI DetourSwapBuffers(HDC hdc) {
    using clock = std::chrono::steady_clock;
    using frames = std::chrono::duration<int, std::ratio<1, 60>>;

    HGLRC context = wglGetCurrentContext();
    if (context) {
        int viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int width  = viewport[2];
        int height = viewport[3];

        auto now  = std::chrono::steady_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::seconds>(now - start);

        if (time.count() > 5) {
            auto wait = WaitForSingleObject(mutex, INFINITE);
            if (wait == WAIT_OBJECT_0) {
                glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);

                currTime = clock::now();
                while (currTime - frameEnd < frames(1)) {
                    currTime = clock::now();
                }
                //if (currTime - frameEnd < frames(1)) {
                //    auto dt = std::chrono::duration<double, std::milli>(currTime - frameEnd);
                //    std::cout << dt.count() << '\n';
                //    //std::this_thread::sleep_until(currTime + frames(1));
                //}

                ReleaseMutex(mutex);
                SetEvent(event);
            }
        }
    } else {
        MessageBoxA(nullptr, "Could not load context\n", "Message", MB_OK);
    }

    frameEnd = clock::now();
    return OriginalSwapBuffers(hdc);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        start = std::chrono::steady_clock::now();

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

        mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, "Local\\VideoFrameMutex");
        if (!mutex) {
            std::cerr << "Error opening video stream mutex\n";
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
