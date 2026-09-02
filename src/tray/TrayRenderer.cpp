#include "tray/TrayRenderer.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace gate
{
namespace
{

using Glyph = std::array<unsigned char, 5>;

Glyph GlyphFor(wchar_t character) noexcept
{
    switch (character)
    {
    case L'0': return {7, 5, 5, 5, 7};
    case L'1': return {2, 6, 2, 2, 7};
    case L'2': return {7, 1, 7, 4, 7};
    case L'3': return {7, 1, 7, 1, 7};
    case L'4': return {5, 5, 7, 1, 1};
    case L'5': return {7, 4, 7, 1, 7};
    case L'6': return {7, 4, 7, 5, 7};
    case L'7': return {7, 1, 2, 2, 2};
    case L'8': return {7, 5, 7, 5, 7};
    case L'9': return {7, 5, 7, 1, 7};
    case L'A': return {2, 5, 7, 5, 5};
    case L'B': return {6, 5, 6, 5, 6};
    case L'C': return {3, 4, 4, 4, 3};
    case L'D': return {6, 5, 5, 5, 6};
    case L'E': return {7, 4, 6, 4, 7};
    case L'F': return {7, 4, 6, 4, 4};
    case L'G': return {3, 4, 5, 5, 3};
    case L'H': return {5, 5, 7, 5, 5};
    case L'I': return {7, 2, 2, 2, 7};
    case L'J': return {1, 1, 1, 5, 2};
    case L'K': return {5, 5, 6, 5, 5};
    case L'L': return {4, 4, 4, 4, 7};
    case L'M': return {5, 7, 7, 5, 5};
    case L'N': return {5, 7, 7, 7, 5};
    case L'O': return {2, 5, 5, 5, 2};
    case L'P': return {6, 5, 6, 4, 4};
    case L'Q': return {2, 5, 5, 3, 1};
    case L'R': return {6, 5, 6, 5, 5};
    case L'S': return {3, 4, 2, 1, 6};
    case L'T': return {7, 2, 2, 2, 2};
    case L'U': return {5, 5, 5, 5, 7};
    case L'V': return {5, 5, 5, 5, 2};
    case L'W': return {5, 5, 7, 7, 5};
    case L'X': return {5, 5, 2, 5, 5};
    case L'Y': return {5, 5, 2, 2, 2};
    case L'Z': return {7, 1, 2, 4, 7};
    case L'-': return {0, 0, 7, 0, 0};
    case L'.': return {0, 0, 0, 0, 2};
    default: return {};
    }
}

void FillLogicalRect(HDC dc, int iconSize, int x, int y, int width, int height, HBRUSH brush)
{
    RECT rectangle{x * iconSize / 16, y * iconSize / 16, (x + width) * iconSize / 16,
                   (y + height) * iconSize / 16};
    if (rectangle.right <= rectangle.left)
    {
        rectangle.right = rectangle.left + 1;
    }
    if (rectangle.bottom <= rectangle.top)
    {
        rectangle.bottom = rectangle.top + 1;
    }
    FillRect(dc, &rectangle, brush);
}

void DrawPixelText(HDC dc, int iconSize, std::wstring_view text, int y, int scaleX, int scaleY, COLORREF color)
{
    if (text.empty())
    {
        return;
    }
    const auto logicalWidth = static_cast<int>(text.size()) * 3 * scaleX +
                              (static_cast<int>(text.size()) - 1) * scaleX;
    const auto startX = std::max(0, (16 - logicalWidth) / 2);
    const auto brush = CreateSolidBrush(color);
    for (std::size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
    {
        const auto glyph = GlyphFor(text[characterIndex]);
        const auto glyphX = startX + static_cast<int>(characterIndex) * 4 * scaleX;
        for (int row = 0; row < 5; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                if ((glyph[static_cast<std::size_t>(row)] & (1U << (2 - column))) != 0)
                {
                    FillLogicalRect(dc, iconSize, glyphX + column * scaleX, y + row * scaleY, scaleX, scaleY,
                                    brush);
                }
            }
        }
    }
    DeleteObject(brush);
}

COLORREF MetricColor(MetricType type, std::optional<double> value) noexcept
{
    if (!value)
    {
        return RGB(75, 79, 86);
    }
    if (type == MetricType::CpuTemperature || type == MetricType::GpuTemperature)
    {
        if (*value < 60.0)
        {
            return RGB(19, 116, 63);
        }
        if (*value < 80.0)
        {
            return RGB(177, 96, 0);
        }
        return RGB(174, 36, 38);
    }
    switch (type)
    {
    case MetricType::CpuUsage:
    case MetricType::CpuClock:
        return RGB(17, 91, 112);
    case MetricType::GpuUsage:
    case MetricType::GpuClock:
    case MetricType::GpuMemoryClock:
    case MetricType::VramUsage:
        return RGB(91, 55, 133);
    case MetricType::RamUsage:
    case MetricType::AvailableRam:
    case MetricType::CommitUsage:
        return RGB(39, 103, 54);
    case MetricType::NetworkDownload:
    case MetricType::NetworkUpload:
        return RGB(35, 77, 137);
    case MetricType::DiskRead:
    case MetricType::DiskWrite:
    case MetricType::DiskUtilization:
        return RGB(139, 75, 16);
    case MetricType::BatteryPercentage:
        return RGB(91, 105, 25);
    case MetricType::CpuTemperature:
    case MetricType::GpuTemperature:
        return RGB(75, 79, 86);
    case MetricType::CpuFanSpeed:
    case MetricType::GpuFanSpeed:
    case MetricType::SystemFanSpeed:
        return RGB(16, 105, 102);
    case MetricType::Count:
        return RGB(75, 79, 86);
    }
    return RGB(75, 79, 86);
}

} // namespace

TrayRenderer::~TrayRenderer()
{
    DestroySurface();
}

void TrayRenderer::Invalidate() noexcept
{
    DestroySurface();
    ++generation_;
}

void TrayRenderer::RefreshTheme()
{
    DWORD value = 1;
    DWORD bytes = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &bytes);
    lightTaskbar_ = value != 0;
}

void TrayRenderer::EnsureSurface(HWND window)
{
    const auto newDpi = window ? GetDpiForWindow(window) : GetDpiForSystem();
    const auto newSize = std::max(16, GetSystemMetricsForDpi(SM_CXSMICON, newDpi));
    if (memoryDc_ && size_ == newSize && dpi_ == newDpi)
    {
        return;
    }
    DestroySurface();
    RefreshTheme();
    dpi_ = newDpi;
    size_ = newSize;

    BITMAPINFO information{};
    information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    information.bmiHeader.biWidth = size_;
    information.bmiHeader.biHeight = -size_;
    information.bmiHeader.biPlanes = 1;
    information.bmiHeader.biBitCount = 32;
    information.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    memoryDc_ = CreateCompatibleDC(screen);
    bitmap_ = CreateDIBSection(screen, &information, DIB_RGB_COLORS, &pixels_, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!memoryDc_ || !bitmap_)
    {
        DestroySurface();
        return;
    }
    oldBitmap_ = SelectObject(memoryDc_, bitmap_);

}

void TrayRenderer::DestroySurface() noexcept
{
    if (memoryDc_ && oldBitmap_)
    {
        SelectObject(memoryDc_, oldBitmap_);
    }
    if (bitmap_)
    {
        DeleteObject(bitmap_);
    }
    if (memoryDc_)
    {
        DeleteDC(memoryDc_);
    }
    memoryDc_ = nullptr;
    bitmap_ = nullptr;
    oldBitmap_ = nullptr;
    pixels_ = nullptr;
    size_ = 0;
    dpi_ = 0;
}

HICON TrayRenderer::Render(const FormattedMetric &metric, MetricType type, std::optional<double> value, HWND window)
{
    EnsureSurface(window);
    if (!memoryDc_ || !pixels_)
    {
        return CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    }
    SecureZeroMemory(pixels_, static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_) * 4U);

    const auto background = MetricColor(type, value);
    const auto brush = CreateSolidBrush(background);
    const auto oldBrush = SelectObject(memoryDc_, brush);
    const auto oldPen = SelectObject(memoryDc_, GetStockObject(NULL_PEN));
    RoundRect(memoryDc_, 0, 0, size_, size_, std::max(3, size_ / 3), std::max(3, size_ / 3));
    SelectObject(memoryDc_, oldPen);
    SelectObject(memoryDc_, oldBrush);
    DeleteObject(brush);

    if (metric.iconLabel.empty())
    {
        const auto scaleX = metric.iconValue.size() <= 2 ? 2 : 1;
        DrawPixelText(memoryDc_, size_, metric.iconValue, 3, scaleX, 2, RGB(255, 255, 255));
    }
    else
    {
        DrawPixelText(memoryDc_, size_, metric.iconLabel, 0, 1, 1, RGB(205, 218, 224));
        const auto scaleX = metric.iconValue.size() <= 2 ? 2 : 1;
        DrawPixelText(memoryDc_, size_, metric.iconValue, 6, scaleX, 2, RGB(255, 255, 255));
    }

    auto *pixels = static_cast<std::uint32_t *>(pixels_);
    for (int y = 0; y < size_; ++y)
    {
        for (int x = 0; x < size_; ++x)
        {
            if ((pixels[y * size_ + x] & 0x00FFFFFFU) != 0)
            {
                pixels[y * size_ + x] |= 0xFF000000U;
            }
        }
    }

    HBITMAP mask = CreateBitmap(size_, size_, 1, 1, nullptr);
    ICONINFO iconInformation{};
    iconInformation.fIcon = TRUE;
    iconInformation.hbmColor = bitmap_;
    iconInformation.hbmMask = mask;
    HICON icon = CreateIconIndirect(&iconInformation);
    DeleteObject(mask);
    return icon;
}

} // namespace gate
