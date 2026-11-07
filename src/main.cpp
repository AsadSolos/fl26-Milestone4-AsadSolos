#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMessageBox>
#include <QString>
#include <QWidget>

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

    aiws::gui::MainWindow window;
    window.set_exit_callback(&confirm_exit);
    window.set_build_delay_ms(build_delay_ms);
    window.show();

    return app.exec();
}
