#include "aiws/gui/ipc_server.hpp"
#include <QDebug>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <cerrno>
#include <cstdint>

namespace aiws::gui {

//constructs the server with a socket path and a callback to add files
IpcServer::IpcServer(const QString& socket_path, AddFileCallback on_add_file, QObject* parent)
    : QObject(parent), socket_path_(socket_path), on_add_file_(std::move(on_add_file)) {}

//stops the server thread and removes the socket file
IpcServer::~IpcServer()
{
    stop();
}

//starts the background thread and binds the socket
bool IpcServer::start()
{
    if (running_) return true;

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0)
    {
        qDebug() << "[ipc] failed to create socket";
        return false;
    }

    //remove existing socket file if it exists
    unlink(socket_path_.toStdString().c_str());

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.toStdString().c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        qDebug() << "[ipc] failed to bind socket" << socket_path_;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 5) < 0)
    {
        qDebug() << "[ipc] failed to listen on socket";
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_ = true;
    thread_ = std::thread(&IpcServer::run_server, this);
    return true;
}

//stops the background thread and removes the socket file
void IpcServer::stop()
{
    qDebug() << "[ipc] stop() called";
    
    //signal the thread to stop
    running_ = false;

    //close the server socket to force accept() to unblock
    if (server_fd_ >= 0)
    {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
        qDebug() << "[ipc] server_fd closed";
    }

    if (thread_.joinable())
    {
        thread_.join();
        qDebug() << "[ipc] thread joined";
    }

    //always try to unlink the socket file
    int result = unlink(socket_path_.toStdString().c_str());
    if (result != 0)
    {
        qDebug() << "[ipc] unlink FAILED:" << strerror(errno);
    }
    else
    {
        qDebug() << "[ipc] unlink SUCCEEDED:" << socket_path_;
    }
}

//the main loop that runs on the background thread
void IpcServer::run_server()
{
    while (running_)
    {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0)
        {
            if (running_) qDebug() << "[ipc] accept failed";
            continue;
        }

        std::string message;
        if (read_request(client_fd, message))
        {
            //message format: "add <path>"
            if (message.rfind("add ", 0) == 0)
            {
                QString path = QString::fromStdString(message.substr(4));
                
                //invoke the callback on the gui thread
                bool success = false;
                QMetaObject::invokeMethod(this, [this, path, &success]() {
                    success = on_add_file_(path);
                }, Qt::BlockingQueuedConnection);

                //reply to the client based on the gui result
                send_reply(client_fd, success ? '0' : '1');
            }
            else
            {
                send_reply(client_fd, '1');
            }
        }
        else
        {
            send_reply(client_fd, '1');
        }

        close(client_fd);
    }
}

//reads a full message from the connected client
bool IpcServer::read_request(int client_fd, std::string& out_message)
{
    //read the 4-byte length prefix
    uint32_t len = 0;
    ssize_t bytes = read(client_fd, &len, sizeof(len));
    if (bytes != sizeof(len))
    {
        return false;
    }

    //read exactly len bytes for the message
    out_message.resize(len);
    size_t total_read = 0;
    
    while (total_read < len)
    {
        bytes = read(client_fd, out_message.data() + total_read, len - total_read);
        if (bytes <= 0)
        {
            return false;
        }
        total_read += bytes;
    }

    return true;
}

//sends a reply byte to the client and closes the connection
void IpcServer::send_reply(int client_fd, char status)
{
    write(client_fd, &status, 1);
}

}  // namespace aiws::gui
