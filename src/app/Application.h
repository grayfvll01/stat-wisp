#pragma once

#include "app/MessageWindow.h"
#include "monitoring/MonitorService.h"
#include "settings/Settings.h"
#include "tray/TrayManager.h"
#include "tray/TrayRenderer.h"

#include <Windows.h>

#include <memory>

namespace gate
{

inline constexpr UINT kSnapshotReadyMessage = WM_APP + 2;
inline constexpr UINT kShowMenuMessage = WM_APP + 3;

class Application final : public MessageHandler
{
  public:
    explicit Application(HINSTANCE instance, bool configureOnly = false);
    ~Application() override;
    [[nodiscard]] int Run();
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, bool &handled) override;

  private:
    void ShowContextMenu(POINT point);
    void HandleCommand(UINT command);
    void ApplySettings(Settings settings);
    void OpenSettings();
    void Exit();

    HINSTANCE instance_{};
    MessageWindow messageWindow_;
    SettingsStore settingsStore_;
    Settings settings_;
    MetricSnapshot latest_;
    TrayRenderer renderer_;
    std::unique_ptr<TrayManager> tray_;
    MonitorService monitor_;
    UINT taskbarCreatedMessage_{};
    bool paused_{};
    bool suspended_{};
    bool exiting_{};
    bool configureOnly_{};
};

} // namespace gate
