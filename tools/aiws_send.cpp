#include "aiws_send.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <cstdint>

namespace aiws {
namespace tools {

//sends a command to the running application via unix domain socket
int send_command(const std::string& socket_path, const std::string& command)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Failed to create socket\n";
        return 2;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "No application could be reached within 2s\n";
        close(sock);
        return 2;
    }

    //send the length of the command as a 4-byte integer
    uint32_t len = static_cast<uint32_t>(command.size());
    if (write(sock, &len, sizeof(len)) != sizeof(len))
    {
        std::cerr << "Failed to send length\n";
        close(sock);
        return 2;
    }

    //send the actual command bytes
    if (write(sock, command.c_str(), len) != len)
    {
        std::cerr << "Failed to send command\n";
        close(sock);
        return 2;
    }

    //read the reply
    char reply = 0;
    if (read(sock, &reply, 1) != 1)
    {
        std::cerr << "Communication failed\n";
        close(sock);
        return 2;
    }
    close(sock);

    return (reply == '0') ? 0 : 1;
}

}  // namespace tools
}  // namespace aiws
