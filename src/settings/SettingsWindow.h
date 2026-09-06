#pragma once

#include "settings/Settings.h"

#include <Windows.h>

namespace statwisp
{

class SettingsWindow final
{
  public:
    [[nodiscard]] static bool Show(HINSTANCE instance, HWND owner, Settings &settings, bool firstRun);
};

} // namespace statwisp
