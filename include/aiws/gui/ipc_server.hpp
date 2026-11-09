#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <functional>
#include <thread>
#include <string>

namespace aiws::gui 
{

//handles posix unix domain socket connections on a background thread
class IpcServer final : public QObject {
    Q_OBJECT

public:
    //callback to pass a received file path to the gui thread
    using AddFileCallback = std::function<bool(const QString& path)>;

    //constructs the server with a socket path and a callback to add files
    explicit IpcServer(const QString& socket_path, AddFileCallback on_add_file, QObject* parent = nullptr);

    //stops the server thread and removes the socket file
    ~IpcServer() override;

    //starts the background thread and binds the socket
    bool start();

    //stops the background thread and unbinds the socket
    void stop();

private:
    //the main loop that runs on the background thread
    void run_server();

    //reads a full message from the connected client
    bool read_request(int client_fd, std::string& out_message);

    //sends a reply byte to the client and closes the connection
    void send_reply(int client_fd, char status);

    QString socket_path_;
    AddFileCallback on_add_file_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    int server_fd_{-1};
};

}  // namespace aiws::gui
