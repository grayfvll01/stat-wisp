#pragma once

#include <Windows.h>

namespace statwisp
{

class MessageHandler
{
  public:
    virtual ~MessageHandler() = default;
    virtual LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, bool &handled) = 0;
};

class MessageWindow final
{
  public:
    MessageWindow() = default;
    ~MessageWindow();
    MessageWindow(const MessageWindow &) = delete;
    MessageWindow &operator=(const MessageWindow &) = delete;

    [[nodiscard]] bool Create(HINSTANCE instance, MessageHandler &handler);
    void Destroy() noexcept;
    [[nodiscard]] HWND Handle() const noexcept
    {
        return window_;
    }
    [[nodiscard]] static const wchar_t *ClassName() noexcept;

  private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    HWND window_{};
    MessageHandler *handler_{};
};

} // namespace statwisp
