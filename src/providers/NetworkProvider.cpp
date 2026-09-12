#include "providers/NetworkProvider.h"

#include <WinSock2.h>
#include <ws2ipdef.h>
#include <Windows.h>
#include <iphlpapi.h>

namespace statwisp
{

void NetworkProvider::Collect(MetricSnapshot &snapshot, bool downloadNeeded, bool uploadNeeded) noexcept
{
    MIB_IF_TABLE2 *table{};
    if (GetIfTable2(&table) != NO_ERROR || !table)
    {
        return;
    }

    std::vector<NetworkInterfaceCounters> counters;
    counters.reserve(table->NumEntries);
    for (ULONG index = 0; index < table->NumEntries; ++index)
    {
        const auto &row = table->Table[index];
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL || row.OperStatus != IfOperStatusUp ||
            row.MediaConnectState != MediaConnectStateConnected)
        {
            continue;
        }
        counters.push_back({row.InterfaceLuid.Value, row.InOctets, row.OutOctets});
    }
    FreeMibTable(table);

    if (const auto rates = rates_.Sample(std::move(counters), std::chrono::steady_clock::now()))
    {
        if (downloadNeeded)
        {
            snapshot.Set(MetricType::NetworkDownload, rates->download);
        }
        if (uploadNeeded)
        {
            snapshot.Set(MetricType::NetworkUpload, rates->upload);
        }
    }
}

void NetworkProvider::Reset() noexcept
{
    rates_.Reset();
}

} // namespace statwisp
