#include "app/MessageWindow.h"

namespace statwisp
{
namespace
{

constexpr wchar_t kMessageWindowClass[] = L"stat-wisp-message-window";

} // namespace

MessageWindow::~MessageWindow()
{
    Destroy();
}

const wchar_t *MessageWindow::ClassName() noexcept
{
    return kMessageWindowClass;
}

bool MessageWindow::Create(HINSTANCE instance, MessageHandler &handler)
{
    handler_ = &handler;
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kMessageWindowClass;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kMessageWindowClass, L"", 0, -32000, -32000, 1, 1,
                              nullptr, nullptr, instance, this);
    return window_ != nullptr;
}

void MessageWindow::Destroy() noexcept
{
    if (window_)
    {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

LRESULT CALLBACK MessageWindow::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *self = reinterpret_cast<MessageWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        self = static_cast<MessageWindow *>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && self->handler_)
    {
        bool handled = false;
        const auto result = self->handler_->HandleMessage(window, message, wParam, lParam, handled);
        if (handled)
        {
            return result;
        }
    }
    if (message == WM_NCDESTROY && self)
    {
        self->window_ = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace statwisp
