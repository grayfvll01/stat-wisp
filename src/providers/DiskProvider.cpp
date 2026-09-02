#include "providers/DiskProvider.h"

#include <Windows.h>
#include <pdh.h>
#include <PdhMsg.h>

#include <algorithm>
#include <optional>

namespace gate
{

struct DiskProvider::Impl
{
    PDH_HQUERY query{};
    PDH_HCOUNTER read{};
    PDH_HCOUNTER write{};
    PDH_HCOUNTER utilization{};
    bool primed{};

    ~Impl()
    {
        if (query)
        {
            PdhCloseQuery(query);
        }
    }

    bool Initialize(bool readNeeded, bool writeNeeded, bool utilizationNeeded)
    {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
        {
            return false;
        }
        if (readNeeded &&
            PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &read) != ERROR_SUCCESS)
        {
            return false;
        }
        if (writeNeeded &&
            PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &write) != ERROR_SUCCESS)
        {
            return false;
        }
        if (utilizationNeeded &&
            PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &utilization) != ERROR_SUCCESS)
        {
            return false;
        }
        return read || write || utilization;
    }

    static std::optional<double> Value(PDH_HCOUNTER counter)
    {
        if (!counter)
        {
            return std::nullopt;
        }
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, nullptr, &value) != ERROR_SUCCESS ||
            (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA))
        {
            return std::nullopt;
        }
        return value.doubleValue;
    }

    bool Collect(MetricSnapshot &snapshot)
    {
        if (PdhCollectQueryData(query) != ERROR_SUCCESS)
        {
            return false;
        }
        if (!primed)
        {
            primed = true;
            return true;
        }
        if (const auto value = Value(read))
        {
            snapshot.Set(MetricType::DiskRead, std::max(0.0, *value));
        }
        if (const auto value = Value(write))
        {
            snapshot.Set(MetricType::DiskWrite, std::max(0.0, *value));
        }
        if (const auto value = Value(utilization))
        {
            snapshot.Set(MetricType::DiskUtilization, std::clamp(*value, 0.0, 100.0));
        }
        return true;
    }
};

DiskProvider::DiskProvider() = default;
DiskProvider::~DiskProvider() = default;

void DiskProvider::Collect(MetricSnapshot &snapshot, bool readNeeded, bool writeNeeded, bool utilizationNeeded) noexcept
{
    if (!impl_)
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < nextInitialize_)
        {
            return;
        }
        auto candidate = std::make_unique<Impl>();
        if (!candidate->Initialize(readNeeded, writeNeeded, utilizationNeeded))
        {
            nextInitialize_ = now + std::chrono::seconds(30);
            return;
        }
        impl_ = std::move(candidate);
    }
    if (!impl_->Collect(snapshot))
    {
        impl_.reset();
        nextInitialize_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }
}

void DiskProvider::Reset() noexcept
{
    impl_.reset();
    nextInitialize_ = {};
}

} // namespace gate
