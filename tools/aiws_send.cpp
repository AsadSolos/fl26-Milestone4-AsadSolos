#include "aiws_send.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

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

    //try to connect
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "No application could be reached within 2s\n";
        close(sock);
        return 2;
    }

    //append newline to the command
    std::string message = command + "\n";

    //send the message
    ssize_t bytes_written = write(sock, message.c_str(), message.size());
    if (bytes_written < 0 || static_cast<size_t>(bytes_written) != message.size())
    {
        std::cerr << "Failed to send command\n";
        close(sock);
        return 2;
    }

    //wait for reply
    char reply = 0;
    ssize_t bytes_read = read(sock, &reply, 1);
    close(sock);

    if (bytes_read != 1)
    {
        std::cerr << "Communication failed\n";
        return 2;
    }

    if (reply == '0')
    {
        return 0; //success
    }
    else
    {
        std::cerr << "Application rejected the request\n";
        return 1;
    }
}

}  // namespace tools
}  // namespace aiws
