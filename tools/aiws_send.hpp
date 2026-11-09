#pragma once

#include <string>

namespace aiws {
namespace tools {

//sends a command to the running application via unix domain socket
//returns 0 on success, 1 on rejection, 2 on connection failure
int send_command(const std::string& socket_path, const std::string& command);

}  // namespace tools
}  // namespace aiws
