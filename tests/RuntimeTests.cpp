#include "providers/GpuProvider.h"
#include "tray/TrayRenderer.h"
#include "core/MetricFormatter.h"
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>

std::vector<unsigned char> Pixels(HICON icon)
{
    ICONINFO info{};
    if (!icon || !GetIconInfo(icon, &info)) throw std::runtime_error("Cannot inspect icon");
    BITMAP bitmap{};
    GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap);
    BITMAPINFO dib{};
    dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dib.bmiHeader.biWidth = bitmap.bmWidth;
    dib.bmiHeader.biHeight = -bitmap.bmHeight;
    dib.bmiHeader.biPlanes = 1;
    dib.bmiHeader.biBitCount = 32;
    std::vector<unsigned char> pixels(static_cast<size_t>(bitmap.bmWidth * bitmap.bmHeight) * 4);
    HDC dc = GetDC(nullptr);
    const auto read = GetDIBits(dc, info.hbmColor, 0, bitmap.bmHeight, pixels.data(), &dib, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    if (!read) throw std::runtime_error("Cannot read icon pixels");
    return pixels;
}

int main()
{
    try {
        statwisp::Settings settings;
        statwisp::TrayRenderer renderer;
        auto render = [&](double value) {
            auto formatted = statwisp::MetricFormatter::Format(statwisp::MetricType::GpuTemperature, value, settings);
            HICON icon = renderer.Render(formatted, statwisp::MetricType::GpuTemperature, value, nullptr);
            auto pixels = Pixels(icon);
            DestroyIcon(icon);
            return pixels;
        };
        const auto cool = render(42);
        const auto warm = render(65);
        const auto hot = render(85);
        if (cool == warm || warm == hot) throw std::runtime_error("Temperature icons did not update");
        const auto gdiBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (int i = 0; i < 500; ++i) {
            if (render(42) != cool || render(85) != hot) throw std::runtime_error("Stale icon pixels");
        }
        if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) != gdiBefore)
            throw std::runtime_error("GDI object leak during repeated rendering");
        statwisp::GpuProvider gpu;
        settings.enabled.fill(false);
        settings.SetEnabled(statwisp::MetricType::GpuTemperature, true);
        for (int i = 0; i < 3; ++i) {
            statwisp::MetricSnapshot snapshot;
            gpu.Collect(snapshot, settings);
            const auto temperature = snapshot.Get(statwisp::MetricType::GpuTemperature);
            std::cout << "GPU temperature sample " << i << ": ";
            if (temperature) std::cout << *temperature << " C\n";
            else std::cout << "unavailable\n";
            Sleep(1000);
        }
        std::cout << "Runtime checks passed; 1,000 icon replacements, stable GDI objects.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
