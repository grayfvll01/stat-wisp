#pragma once

#include "monitoring/MetricSnapshot.h"
#include "settings/Settings.h"

#include <Windows.h>

namespace gate
{

class AmdProvider final
{
  public:
    AmdProvider() = default;
    ~AmdProvider();
    AmdProvider(const AmdProvider &) = delete;
    AmdProvider &operator=(const AmdProvider &) = delete;

    [[nodiscard]] bool Initialize() noexcept;
    [[nodiscard]] bool Available() const noexcept
    {
        return context_ != nullptr && adapterIndex_ >= 0;
    }
    void Collect(MetricSnapshot &snapshot, const Settings &settings) noexcept;
    void Reset() noexcept;

  private:
    using Context = void *;
    using AllocFn = void *(__stdcall *)(int);
    using CreateFn = int (*)(AllocFn, int, Context *);
    using DestroyFn = int (*)(Context);
    using CountFn = int (*)(Context, int *);
    using InfoFn = int (*)(Context, void *, int);
    using CapsFn = int (*)(Context, int, int *, int *, int *);
    using ActivityFn = int (*)(Context, int, void *);
    using TemperatureFn = int (*)(Context, int, int, void *);
    using FanSpeedFn = int (*)(Context, int, int, void *);

    HMODULE library_{};
    Context context_{};
    DestroyFn destroy_{};
    ActivityFn activity_{};
    TemperatureFn temperature_{};
    FanSpeedFn fanSpeed_{};
    int adapterIndex_{-1};
    bool attempted_{};
};

} // namespace gate
