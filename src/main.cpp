#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <cstdlib>
#include <unistd.h>

//asks the user to confirm exit and quits on yes
void confirm_exit(QWidget* parent)
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        parent,
        QStringLiteral("Exit"),
        QStringLiteral("Quit the application?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer == QMessageBox::Yes)
    {
        QCoreApplication::quit();
    }
}

//gets the default socket path
QString get_default_socket_path()
{
    const char* user = std::getenv("USER");
    QString user_str;
    
    if (user != nullptr)
    {
        user_str = QString::fromUtf8(user);
    }
    else
    {
        user_str = QString::number(getuid());
    }

    return QStringLiteral("/tmp/aiws-%1.sock").arg(user_str);
}

//creates the application window registers the exit callback and runs the loop
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AI Workspace"));
    QCoreApplication::setApplicationVersion(QStringLiteral("4.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AI Workspace M4"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption delayOption(QStringList() << QStringLiteral("build-delay-ms"), QStringLiteral("Extra build time per document in milliseconds."), QStringLiteral("ms"));
    parser.addOption(delayOption);

    QCommandLineOption socketOption(QStringList() << QStringLiteral("ipc-socket"), QStringLiteral("Path to the IPC socket."), QStringLiteral("path"));
    parser.addOption(socketOption);

    parser.process(app);

    int build_delay_ms = 0;
    if (parser.isSet(delayOption))
    {
        bool ok = false;
        const int value = parser.value(delayOption).toInt(&ok);
        if (ok && value >= 0)
        {
            build_delay_ms = value;
        }
    }

    QString socket_path = get_default_socket_path();
    if (parser.isSet(socketOption))
    {
        socket_path = parser.value(socketOption);
    }

    aiws::gui::MainWindow window;
    window.set_exit_callback(&confirm_exit);
    window.set_build_delay_ms(build_delay_ms);
    window.set_ipc_socket(socket_path);
    window.show();

    return app.exec();
}
