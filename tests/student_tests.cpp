#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QTemporaryDir>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QTextStream>
#include <QThread>
#include <QLineEdit>

#include "aiws/gui/main_window.hpp"
#include "aiws/gui/workspace_widget.hpp"

using aiws::gui::MainWindow;
using aiws::gui::WorkspaceWidget;

class M4StudentTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testR1Responsiveness();
    void testR2CorrectnessStaleResult();
    void testR3Failure();
    void testR4Lifecycle();

private:
    QTemporaryDir m_tempDir;
    QString m_file1;
    QString m_file2;
    QString m_file3;

    //helper to write dummy text files
    QString createTestFile(const QString& name, const QString& content);

    //helper to run the event loop for a bit so the background thread can finish
    void waitForBuild(int ms = 100);
};

QString M4StudentTests::createTestFile(const QString& name, const QString& content)
{
    QString path = m_tempDir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << content;
        file.close();
    }
    return path;
}

void M4StudentTests::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_file1 = createTestFile("doc1.txt", "retrieval index term frequency");
    m_file2 = createTestFile("doc2.txt", "context token budget ranking");
    m_file3 = createTestFile("doc3.txt", "unknownword synchronization");
}

void M4StudentTests::waitForBuild(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(10);
    }
}

//r1 responsiveness the gui remains responsive during a build
void M4StudentTests::testR1Responsiveness()
{
    MainWindow window;
    QVERIFY(window.add_document_file(m_file1));

    //set a long delay so the build takes a while
    window.set_build_delay_ms(500);

    //find the build button and click it
    QPushButton* buildBtn = nullptr;
    for (auto* btn : window.findChildren<QPushButton*>())
    {
        if (btn->text() == "Build Corpus")
        {
            buildBtn = btn;
            break;
        }
    }
    QVERIFY(buildBtn != nullptr);
    QTest::mouseClick(buildBtn, Qt::LeftButton);

    //immediately check that the workspace label says "Building"
    WorkspaceWidget* wsWidget = window.findChild<WorkspaceWidget*>();
    QVERIFY(wsWidget != nullptr);
    
    bool foundBuilding = false;
    for (auto* lbl : wsWidget->findChildren<QLabel*>())
    {
        if (lbl->text().contains("Building"))
        {
            foundBuilding = true;
            break;
        }
    }
    QVERIFY(foundBuilding);

    //wait for the build to finish to clean up
    waitForBuild(1000);
}

//r2 correctness stale results are discarded when workspace changes
void M4StudentTests::testR2CorrectnessStaleResult()
{
    MainWindow window;
    QVERIFY(window.add_document_file(m_file1));

    //type a query so the search button can become enabled
    QLineEdit* queryEdit = window.findChild<QLineEdit*>();
    QVERIFY(queryEdit != nullptr);
    queryEdit->setText("retrieval");

    //start a long build
    window.set_build_delay_ms(300);
    
    QPushButton* buildBtn = nullptr;
    for (auto* btn : window.findChildren<QPushButton*>())
    {
        if (btn->text() == "Build Corpus")
        {
            buildBtn = btn;
            break;
        }
    }
    QVERIFY(buildBtn != nullptr);
    QTest::mouseClick(buildBtn, Qt::LeftButton);

    //wait a tiny bit to ensure the thread has started
    waitForBuild(50);

    //now invalidate the corpus by adding a document
    QVERIFY(window.add_document_file(m_file2));

    //wait for the build to finish
    waitForBuild(600);

    //the build result should have been thrown away
    //search should still be disabled
    QPushButton* searchBtn = nullptr;
    for (auto* btn : window.findChildren<QPushButton*>())
    {
        if (btn->text() == "Search")
        {
            searchBtn = btn;
            break;
        }
    }
    QVERIFY(searchBtn != nullptr);
    
    //if AIWS_FAULT_ACCEPT_STALE is defined, the search button will be enabled
    QVERIFY(!searchBtn->isEnabled());
}

//r3 failure a failed build is reported and the app remains usable
void M4StudentTests::testR3Failure()
{
    MainWindow window;
    QVERIFY(window.add_document_file(m_file1));

    //add the same file twice to cause a duplicate id failure in processing core
    QVERIFY(window.add_document_file(m_file1));

    QPushButton* buildBtn = nullptr;
    for (auto* btn : window.findChildren<QPushButton*>())
    {
        if (btn->text() == "Build Corpus")
        {
            buildBtn = btn;
            break;
        }
    }
    QVERIFY(buildBtn != nullptr);
    QTest::mouseClick(buildBtn, Qt::LeftButton);

    waitForBuild(200);

    //the build should fail and the status should show an error
    QLabel* status = nullptr;
    for (auto* lbl : window.findChildren<QLabel*>())
    {
        if (lbl->text().contains("Current status:"))
        {
            status = lbl;
            break;
        }
    }
    QVERIFY(status != nullptr);

    //if AIWS_FAULT_LOSE_FAILURE is defined the status will say "built" and the test fails
    QVERIFY(status->text().contains("failed") || status->text().contains("Not built"));
}

//r4 lifecycle repeated builds do not accumulate threads or crash
void M4StudentTests::testR4Lifecycle()
{
    MainWindow window;
    QVERIFY(window.add_document_file(m_file1));
    window.set_build_delay_ms(50);

    QPushButton* buildBtn = nullptr;
    for (auto* btn : window.findChildren<QPushButton*>())
    {
        if (btn->text() == "Build Corpus")
        {
            buildBtn = btn;
            break;
        }
    }
    QVERIFY(buildBtn != nullptr);

    //run 5 builds in a row
    for (int i = 0; i < 5; ++i)
    {
        QTest::mouseClick(buildBtn, Qt::LeftButton);
        waitForBuild(100);
    }

    //if we get here without crashing or hanging the lifecycle is good
    QVERIFY(true);
}

QTEST_MAIN(M4StudentTests)
#include "student_tests.moc"
