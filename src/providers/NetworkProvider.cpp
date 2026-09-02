#include "providers/NetworkProvider.h"

#include <WinSock2.h>
#include <ws2ipdef.h>
#include <Windows.h>
#include <iphlpapi.h>

namespace gate
{

void NetworkProvider::Collect(MetricSnapshot &snapshot, bool downloadNeeded, bool uploadNeeded) noexcept
{
    MIB_IF_TABLE2 *table{};
    if (GetIfTable2(&table) != NO_ERROR || !table)
    {
        return;
    }

    std::uint64_t received = 0;
    std::uint64_t sent = 0;
    for (ULONG index = 0; index < table->NumEntries; ++index)
    {
        const auto &row = table->Table[index];
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL || row.OperStatus != IfOperStatusUp ||
            row.MediaConnectState != MediaConnectStateConnected)
        {
            continue;
        }
        received += row.InOctets;
        sent += row.OutOctets;
    }
    FreeMibTable(table);

    const auto now = std::chrono::steady_clock::now();
    if (hasPrevious_ && received >= previousReceived_ && sent >= previousSent_)
    {
        const auto seconds = std::chrono::duration<double>(now - previousTime_).count();
        if (seconds > 0.0)
        {
            if (downloadNeeded)
            {
                snapshot.Set(MetricType::NetworkDownload, static_cast<double>(received - previousReceived_) / seconds);
            }
            if (uploadNeeded)
            {
                snapshot.Set(MetricType::NetworkUpload, static_cast<double>(sent - previousSent_) / seconds);
            }
        }
    }
    previousReceived_ = received;
    previousSent_ = sent;
    previousTime_ = now;
    hasPrevious_ = true;
}

void NetworkProvider::Reset() noexcept
{
    previousReceived_ = previousSent_ = 0;
    previousTime_ = {};
    hasPrevious_ = false;
}

} // namespace gate
