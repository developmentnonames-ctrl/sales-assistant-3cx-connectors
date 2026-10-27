#include "process_info.h"

#include <vector>

#include "win32.h"

#include <iphlpapi.h>
#include <tlhelp32.h>

#include "handle.h"

namespace sa3cx {

namespace {

// Calls `visit` for each running process until it returns true.
template <typename Visit>
void ForEachProcess(Visit visit) {
  UniqueHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.valid()) return;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof entry;
  if (!Process32FirstW(snapshot.get(), &entry)) return;
  do {
    if (visit(entry)) return;
  } while (Process32NextW(snapshot.get(), &entry));
}

}  // namespace

uint32_t FindProcessByName(const std::wstring& exe_name) {
  uint32_t found = 0;
  ForEachProcess([&](const PROCESSENTRY32W& entry) {
    if (_wcsicmp(entry.szExeFile, exe_name.c_str()) != 0) return false;
    found = entry.th32ProcessID;
    return true;
  });
  return found;
}

std::wstring ProcessName(uint32_t process_id) {
  std::wstring name;
  ForEachProcess([&](const PROCESSENTRY32W& entry) {
    if (entry.th32ProcessID != process_id) return false;
    name = entry.szExeFile;
    return true;
  });
  return name;
}

bool ProcessExists(uint32_t process_id) {
  UniqueHandle process(OpenProcess(SYNCHRONIZE, FALSE, process_id));
  return process.valid() && WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT;
}

uint32_t LoopbackPeerProcess(unsigned short client_port_net, unsigned short server_port_net) {
  ULONG size = 0;
  GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0);
  std::vector<BYTE> buffer(size + 4096);  // room for connections opened in between
  size = static_cast<ULONG>(buffer.size());
  if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS,
                          0) != NO_ERROR) {
    return 0;
  }
  const auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
  for (DWORD i = 0; i < table->dwNumEntries; ++i) {
    const MIB_TCPROW_OWNER_PID& row = table->table[i];
    // The client's own row: its local end is 127.0.0.1:<client port>, its remote end our port.
    if (row.dwLocalAddr == htonl(INADDR_LOOPBACK) &&
        static_cast<unsigned short>(row.dwLocalPort) == client_port_net &&
        static_cast<unsigned short>(row.dwRemotePort) == server_port_net) {
      return row.dwOwningPid;
    }
  }
  return 0;
}

bool IsInOurSession(uint32_t process_id) {
  DWORD ours = 0;
  DWORD theirs = 0;
  if (process_id == 0) return false;
  if (!ProcessIdToSessionId(GetCurrentProcessId(), &ours)) return false;
  if (!ProcessIdToSessionId(process_id, &theirs)) return false;
  return ours == theirs;
}

}  // namespace sa3cx
