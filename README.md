# Graphics Development Server

[![test video](test_thumbnail.png)](test.mp4)

This is an application which is meant to fill a gap in my remote workflow. Working remotely through Remote Desktop or Parsec is cumbersome and X11 forwarding through SSH doesn't work on Windows (as far as I found). So, this is my first attempt at forwarding an application from a remote computer to a client. 

The current implementation doesn't include any of the screen casting logic, as I didn't get to it. I managed to record the frames of an OpenGL application by using DLL injection and function hooking, which only makes it useful for OpenGL applications. I later found that windows offers a capture API, as such I will be continuing the project under a different repository using that. At the present time this is just a demo, recording the frames into a .mp4 container using FFmpeg. 

## Requirements

- CMake 3.30
- MinGW 13.2.0 MSVCRT (or equivalent)
- vcpkg

## Dependencies

- FFmpeg
- minhook
- Boost ASIO (not used)

## Building

```
git clone github.com/Rocco2300/graphics-dev-server

cd graphics-dev-server
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=OFF -DCMAKE_TOOLCHAIN_FILE=<path_to_vckpg.cmake> -DVCPKG_TARGET_TRIPLET=x64-mingw-static
cmake --build .

cd ../bin
./graphics-dev-srv.exe <path_to_app>
```

Keep in mind that you will have to modify the path from server.cpp to point to your bin directory.