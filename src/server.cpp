#include <iostream>
#include <string>

#include <windows.h>

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
    }

    LPVOID remoteBuffer = VirtualAllocEx(pi.hProcess,
                                         nullptr,
                                         dllNameLen,
                                         MEM_RESERVE | MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    if (!remoteBuffer) {
        std::cerr << "Error allocating memory in remote process\n";
    }

    if (!WriteProcessMemory(pi.hProcess, remoteBuffer, dllName, dllNameLen, nullptr)) {
        std::cerr << "Could not write memory into remote process\n";
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
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(threadHandle);

    return 0;
}