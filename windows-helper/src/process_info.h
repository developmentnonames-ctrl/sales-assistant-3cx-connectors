// Questions about other processes: by name, by ID, and who owns the other end of a local TCP
// connection.
#ifndef SA3CX_PROCESS_INFO_H_
#define SA3CX_PROCESS_INFO_H_

#include <cstdint>
#include <string>

namespace sa3cx {

// The ID of the first process with this executable name (case-insensitive), or 0.
uint32_t FindProcessByName(const std::wstring& exe_name);

// The executable name of a process, or "" if it is gone.
std::wstring ProcessName(uint32_t process_id);

// True while the process exists.
bool ProcessExists(uint32_t process_id);

// The process that owns the client end of a connection to 127.0.0.1:`server_port_net`, found by
// the client's port. Ports in network byte order. 0 if not found.
uint32_t LoopbackPeerProcess(unsigned short client_port_net, unsigned short server_port_net);

// True if the process runs in the same Windows session (signed-in user) as this program.
bool IsInOurSession(uint32_t process_id);

}  // namespace sa3cx

#endif  // SA3CX_PROCESS_INFO_H_
