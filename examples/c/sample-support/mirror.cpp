// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/sample_support.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <stdexcept>
extern thread_local std::string sampleError;
#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
namespace
{
struct Mirror
{
    HWND window = nullptr;
    std::vector<uint8_t> bgra;
    BITMAPINFO bitmap{};
    int key = 0;
    ~Mirror() { if (window) DestroyWindow(window); }
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_NCCREATE)
            SetWindowLongPtrW(window, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
        auto* self = reinterpret_cast<Mirror*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (self)
        {
            if (message == WM_CHAR) { self->key = int(wparam); return 0; }
            if (message == WM_DESTROY) { self->window = nullptr; return 0; }
            if (message == WM_ERASEBKGND) return 1;
            if (message == WM_PAINT)
            {
                PAINTSTRUCT paint{};
                HDC dc = BeginPaint(window, &paint);
                RECT bounds{};
                GetClientRect(window, &bounds);
                FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                if (!self->bgra.empty())
                {
                    const int width = self->bitmap.bmiHeader.biWidth, height = -self->bitmap.bmiHeader.biHeight;
                    const double scale = (std::min)(double(bounds.right) / width, double(bounds.bottom) / height);
                    const int w = int(width * scale), h = int(height * scale);
                    SetStretchBltMode(dc, COLORONCOLOR);
                    StretchDIBits(dc, (bounds.right - w) / 2, (bounds.bottom - h) / 2, w, h,
                                  0, 0, width, height, self->bgra.data(), &self->bitmap, DIB_RGB_COLORS, SRCCOPY);
                }
                EndPaint(window, &paint);
                return 0;
            }
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
    void events()
    {
        MSG event{};
        while (window && PeekMessageW(&event, window, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&event);
            DispatchMessageW(&event);
        }
    }
};
std::wstring wide(const char* text)
{
    const int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (!count) throw std::runtime_error("Invalid window title");
    std::wstring value(size_t(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, value.data(), count);
    return value;
}
}
extern "C" void* sample_mirror_create(const char* title)
{
    try
    {
        WNDCLASSW type{};
        type.lpfnWndProc = Mirror::procedure;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"OvxrPublicSampleMirror";
        type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("RegisterClass failed");
        auto mirror = std::make_unique<Mirror>();
        mirror->window = CreateWindowExW(0, type.lpszClassName, wide(title).c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1000, 800, nullptr, nullptr, type.hInstance, mirror.get());
        if (!mirror->window) throw std::runtime_error("CreateWindow failed");
        ShowWindow(mirror->window, SW_SHOW);
        return mirror.release();
    }
    catch (const std::exception& error) { sampleError = error.what(); return nullptr; }
}
extern "C" void* sample_pip_create(const char* title)
{
    auto* mirror = static_cast<Mirror*>(sample_mirror_create(title));
    if (!mirror) return nullptr;
    // Do not steal keyboard focus while arranging the small desktop preview.
    if (!SetWindowPos(mirror->window, HWND_TOPMOST, 0, 0, 520, 440, SWP_NOMOVE | SWP_NOACTIVATE))
    {
        sampleError = "Could not position desktop PiP";
        delete mirror;
        return nullptr;
    }
    return mirror;
}
extern "C" int sample_mirror_present(void* pointer, const void* rgba, uint32_t width, uint32_t height, uint32_t pitch)
{
    try
    {
        auto& mirror = *static_cast<Mirror*>(pointer);
        mirror.events();
        if (!mirror.window) return 0;
        if (rgba)
        {
            if (!width || !height || uint64_t(width) * 4 > pitch || uint64_t(width) * height > 100000000)
                throw std::runtime_error("Invalid mirror image layout");
            mirror.bgra.resize(size_t(width) * height * 4);
            auto* source = static_cast<const uint8_t*>(rgba);
            for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    auto* out = mirror.bgra.data() + (size_t(y) * width + x) * 4;
                    const auto* in = source + size_t(y) * pitch + x * 4;
                    out[0] = in[2]; out[1] = in[1]; out[2] = in[0]; out[3] = 0;
                }
            auto& info = mirror.bitmap.bmiHeader;
            info.biSize = sizeof(info); info.biWidth = int(width); info.biHeight = -int(height);
            info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
            InvalidateRect(mirror.window, nullptr, FALSE);
            UpdateWindow(mirror.window);
        }
        return 1;
    }
    catch (const std::exception& error) { sampleError = error.what(); return -1; }
}
extern "C" int sample_mirror_key(void* pointer)
{
    auto& mirror = *static_cast<Mirror*>(pointer);
    mirror.events();
    const int key = mirror.key;
    mirror.key = 0;
    return key;
}
extern "C" void sample_mirror_title(void* pointer, const char* title)
{
    auto& mirror = *static_cast<Mirror*>(pointer);
    if (mirror.window) SetWindowTextW(mirror.window, wide(title).c_str());
}
extern "C" void sample_mirror_destroy(void* pointer) { delete static_cast<Mirror*>(pointer); }
#else
extern "C" void* sample_mirror_create(const char*)
{
    sampleError = "The example desktop mirror uses Win32. Omit --mirror on this platform.";
    return nullptr;
}
extern "C" void* sample_pip_create(const char*)
{
    sampleError = "Desktop PiP uses Win32. Use --pip headset or --pip off on this platform.";
    return nullptr;
}
extern "C" int sample_mirror_present(void*, const void*, uint32_t, uint32_t, uint32_t) { return -1; }
extern "C" int sample_mirror_key(void*) { return 0; }
extern "C" void sample_mirror_title(void*, const char*) {}
extern "C" void sample_mirror_destroy(void*) {}
#endif
