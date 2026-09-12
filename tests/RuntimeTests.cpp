#include "providers/GpuProvider.h"
#include "tray/TrayRenderer.h"
#include "core/MetricFormatter.h"
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>

void TestSettingsPersistence()
{
    // A relative override must work without a parent directory component.
    const std::filesystem::path path = "stat-wisp-runtime-settings-" + std::to_string(GetCurrentProcessId()) + ".ini";
    auto temporary = path;
    temporary += L".tmp";
    struct Cleanup
    {
        const std::filesystem::path &path;
        const std::filesystem::path &temporary;
        ~Cleanup()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            std::filesystem::remove(temporary, ignored);
        }
    } cleanup{path, temporary};

    statwisp::SettingsStore store(path);
    statwisp::Settings settings;
    settings.firstRunCompleted = true;
    settings.updateIntervalMs = 30000;
    if (!store.Save(settings)) throw std::runtime_error("Relative settings save failed");
    if (store.Load().updateIntervalMs != 30000) throw std::runtime_error("Settings were not persisted");

    // Denying delete sharing prevents atomic replacement, as can happen while
    // another program holds the settings file open. Keep the old file intact.
    HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (locked == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot lock settings for failure test");
    settings.updateIntervalMs = 500;
    const bool saved = store.Save(settings);
    CloseHandle(locked);
    if (saved) throw std::runtime_error("Replacing locked settings incorrectly succeeded");
    if (store.Load().updateIntervalMs != 30000) throw std::runtime_error("Failed save damaged previous settings");
    if (std::filesystem::exists(temporary)) throw std::runtime_error("Failed save left a temporary settings file");
    if (!store.Save(settings) || store.Load().updateIntervalMs != 500)
        throw std::runtime_error("Settings could not be saved after the lock was released");

    for (const auto version : {"", "garbage", "-1", "4294967296"})
    {
        bool valid = true;
        bool migrated = true;
        const auto loaded = statwisp::SettingsCodec::Deserialize(
            std::string("version=") + version + "\nenabled=cpu.usage\nfirst_run_completed=1\n", &migrated, &valid);
        if (valid || migrated || loaded.firstRunCompleted)
            throw std::runtime_error("Corrupt settings version was accepted as a legacy version");
    }
}

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
        TestSettingsPersistence();
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
