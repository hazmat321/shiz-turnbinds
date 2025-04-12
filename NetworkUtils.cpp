#include "NetworkUtils.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <memory>
#include <algorithm>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

bool isConnectedToServer(const std::vector<std::string>& targetIPs) {
    if (targetIPs.empty()) return true;
    PMIB_TCPTABLE2 tcpTable = nullptr;
    ULONG size = 0;
    DWORD result = GetTcpTable2(tcpTable, &size, FALSE);
    if (result == ERROR_INSUFFICIENT_BUFFER) {
        tcpTable = (PMIB_TCPTABLE2)malloc(size);
        if (!tcpTable) return false;
        result = GetTcpTable2(tcpTable, &size, FALSE);
    }
    if (result != NO_ERROR) {
        free(tcpTable);
        return false;
    }
    std::unique_ptr<MIB_TCPTABLE2, void(*)(void*)> tableGuard(tcpTable, free);

    bool found = false;
    for (DWORD i = 0; i < tableGuard->dwNumEntries; ++i) {
        MIB_TCPROW2 row = tableGuard->table[i];
        if (row.dwState == MIB_TCP_STATE_ESTAB) {
            char remoteAddr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &row.dwRemoteAddr, remoteAddr, sizeof(remoteAddr))) {
                if (std::find(targetIPs.begin(), targetIPs.end(), remoteAddr) != targetIPs.end()) {
                    found = true;
                    break;
                }
            }
        }
    }
    return found;
}