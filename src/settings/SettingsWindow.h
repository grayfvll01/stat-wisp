#pragma once

#include "settings/Settings.h"

#include <Windows.h>

namespace gate
{

class SettingsWindow final
{
  public:
    [[nodiscard]] static bool Show(HINSTANCE instance, HWND owner, Settings &settings, bool firstRun);
};

} // namespace gate
