#include "providers/GpuPerformanceProvider.h"
#include "core/GpuEngineUsage.h"

#include <Windows.h>
#include <pdh.h>
#include <PdhMsg.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace statwisp
{

struct GpuPerformanceProvider::Impl
{
    PDH_HQUERY query{};
    PDH_HCOUNTER counter{};
    bool primed{};

    ~Impl()
    {
        if (query)
        {
            PdhCloseQuery(query);
        }
    }

    bool Initialize()
    {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
        {
            return false;
        }
        constexpr wchar_t path[] = L"\\GPU Engine(*)\\Utilization Percentage";
        return PdhAddEnglishCounterW(query, path, 0, &counter) == ERROR_SUCCESS;
    }

    std::optional<double> Read()
    {
        if (PdhCollectQueryData(query) != ERROR_SUCCESS)
        {
            return std::nullopt;
        }
        if (!primed)
        {
            primed = true;
            return std::nullopt;
        }
        DWORD bytes = 0;
        DWORD count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, nullptr) !=
                PDH_MORE_DATA ||
            bytes == 0)
        {
            return std::nullopt;
        }
        std::vector<std::byte> buffer(bytes);
        auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, items) !=
            ERROR_SUCCESS)
        {
            return std::nullopt;
        }
        GpuEngineUsage engines;
        for (DWORD index = 0; index < count; ++index)
        {
            if (items[index].szName &&
                (items[index].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA ||
                 items[index].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA))
            {
                engines.Add(items[index].szName, items[index].FmtValue.doubleValue);
            }
        }
        return engines.Busiest();
    }
};

GpuPerformanceProvider::GpuPerformanceProvider() = default;
GpuPerformanceProvider::~GpuPerformanceProvider() = default;

std::optional<double> GpuPerformanceProvider::ReadUsage() noexcept
{
    if (!impl_)
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < nextInitialize_)
        {
            return std::nullopt;
        }
        auto candidate = std::make_unique<Impl>();
        if (!candidate->Initialize())
        {
            nextInitialize_ = now + std::chrono::seconds(30);
            return std::nullopt;
        }
        impl_ = std::move(candidate);
    }
    return impl_->Read();
}

void GpuPerformanceProvider::Reset() noexcept
{
    impl_.reset();
    nextInitialize_ = {};
}

} // namespace statwisp
