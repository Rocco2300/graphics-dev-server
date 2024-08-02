#include "MinHook.h"

#include <gl/gl.h>
#include <windows.h>

#include <cinttypes>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>

typedef BOOL(WINAPI* SwapBuffersType)(HDC hdc);

SwapBuffersType OriginalSwapBuffers = nullptr;

static bool saved{};
static std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> start;

BOOL WINAPI DetourSwapBuffers(HDC hdc) {
    HGLRC context = wglGetCurrentContext();
    if (context) {
        int viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int width  = viewport[2];
        int height = viewport[3];
        std::vector<uint8_t> pixels(3 * width * height);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

        auto now = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (!saved && time.count() >= 5.0) {
            MessageBoxA(nullptr, "Saved a screen capture", "Message", MB_OK);
            std::ofstream out("C:\\Users\\grigo\\Desktop\\test.ppm");
            out << "P3\n";
            out << width << ' ' << height << '\n';
            out << "255\n";
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    int index      = (i * width + j) * 3;
                    uint8_t* pixel = &pixels[index];
                    out << static_cast<int>(pixel[0]) << ' ' << static_cast<int>(pixel[1]) << ' '
                        << static_cast<int>(pixel[2]) << '\n';
                }
            }
            out.close();
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
        start = std::chrono::high_resolution_clock::now();
        if (MH_Initialize() != MH_OK) {
            std::cerr << "Could not init minhook\n";
            return 1;
        }

        if (MH_CreateHookApi(L"gdi32", "SwapBuffers", (void*) &DetourSwapBuffers,
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
        break;
    }
    return TRUE;
}
