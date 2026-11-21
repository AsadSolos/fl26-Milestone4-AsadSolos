#include <QtTest/QtTest>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "aiws/gui/build_worker.hpp"
#include "aiws/gui/main_window.hpp"
#include "aiws/workspace.hpp"

using aiws::Document;
using aiws::Workspace;
using aiws::gui::BuildWorker;
using aiws::gui::MainWindow;

namespace {

//counts finished builds and remembers which thread reported each one
class DoneCounter
{
public:
    void notify()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++count_;
            thread_ids_.push_back(std::this_thread::get_id());
        }
        cv_.notify_all();
    }

    //waits on a condition variable until the count reaches the target or the time runs out
    bool wait_for_count(int target, int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, target] { return count_ >= target; });
    }

    int count()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    std::vector<std::thread::id> thread_ids()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return thread_ids_;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_{0};
    std::vector<std::thread::id> thread_ids_;
};

//ends the whole process with a failure if a shutdown does not finish in time
class ExitWatchdog
{
public:
    explicit ExitWatchdog(int limit_ms) : thread_(&ExitWatchdog::run, this, limit_ms) {}

    ~ExitWatchdog()
    {
        finish();
    }

    //called by the test thread once the shutdown returned
    void finish()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            finished_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

private:
    void run(int limit_ms)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::milliseconds(limit_ms), [this] { return finished_; }))
        {
            std::fputs("FAIL: shutdown did not finish in time\n", stderr);
            std::_Exit(1);
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    bool finished_{false};
    std::thread thread_;
};

//builds a workspace with the given number of small documents
Workspace make_workspace(int documents)
{
    Workspace workspace;
    for (int i = 0; i < documents; ++i)
    {
        workspace.add_document(Document("doc" + std::to_string(i), "title", "retrieval index term frequency " + std::to_string(i)));
    }
    return workspace;
}

//finds a button by its label
QPushButton* find_button(MainWindow& window, const QString& text)
{
    for (auto* button : window.findChildren<QPushButton*>())
    {
        if (button->text() == text)
        {
            return button;
        }
    }
    return nullptr;
}

//finds the first label that contains the given text
QString find_label_text(MainWindow& window, const QString& part)
{
    for (auto* label : window.findChildren<QLabel*>())
    {
        if (label->text().contains(part))
        {
            return label->text();
        }
    }
    return QString();
}

//finds the query field of the search widget
QLineEdit* find_query_edit(MainWindow& window)
{
    for (auto* edit : window.findChildren<QLineEdit*>())
    {
        if (edit->placeholderText() == QStringLiteral("Search the current corpus"))
        {
            return edit;
        }
    }
    return nullptr;
}

}  // namespace

class M5StudentTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void worker_uses_one_thread_for_all_builds();
    void worker_stops_cancelled_build_quickly();
    void worker_never_reports_cancelled_build();
    void worker_shuts_down_when_idle();
    void worker_shuts_down_mid_build();

    void build_does_not_block_gui();
    void build_becomes_current_and_enables_search();
    void reset_cancels_running_build();
    void second_build_request_replaces_running_build();
    void workspace_change_cancels_running_build();
    void finished_build_is_dropped_after_workspace_change();
    void failed_build_returns_to_not_built();
    void exit_stops_running_build();
    void exit_works_when_no_build_is_running();

private:
    QTemporaryDir temp_dir_;
    QStringList files_;

    //ends the process with a failure if any single test hangs including its scope exit shutdowns
    std::unique_ptr<ExitWatchdog> test_watchdog_;

    //adds all six test files to the window
    void add_all_files(MainWindow& window);

    //clicks build corpus and returns how long the click took
    qint64 click_build(MainWindow& window);
};

void M5StudentTests::initTestCase()
{
    QVERIFY(temp_dir_.isValid());
    for (int i = 0; i < 6; ++i)
    {
        const QString path = temp_dir_.filePath(QStringLiteral("doc%1.txt").arg(i));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "retrieval index term frequency " << i;
        file.close();
        files_ << path;
    }
}

void M5StudentTests::init()
{
    test_watchdog_ = std::make_unique<ExitWatchdog>(8000);
}

void M5StudentTests::cleanup()
{
    test_watchdog_.reset();
}

void M5StudentTests::add_all_files(MainWindow& window)
{
    for (const QString& path : files_)
    {
        QVERIFY(window.add_document_file(path));
    }
}

qint64 M5StudentTests::click_build(MainWindow& window)
{
    QPushButton* build_button = find_button(window, QStringLiteral("Build Corpus"));
    if (build_button == nullptr)
    {
        return -1;
    }
    QElapsedTimer timer;
    timer.start();
    QTest::mouseClick(build_button, Qt::LeftButton);
    return timer.elapsed();
}

//r1 every build is reported from the same single worker thread
void M5StudentTests::worker_uses_one_thread_for_all_builds()
{
    DoneCounter counter;
    BuildWorker worker(std::bind(&DoneCounter::notify, &counter));

    for (int i = 1; i <= 4; ++i)
    {
        worker.submit(make_workspace(1), 0);
        QVERIFY(counter.wait_for_count(i, 2000));
        QVERIFY(worker.take_outcome() != nullptr);
    }

    const std::vector<std::thread::id> ids = counter.thread_ids();
    QCOMPARE(static_cast<int>(ids.size()), 4);
    QVERIFY(ids[0] != std::this_thread::get_id());
    for (const std::thread::id& id : ids)
    {
        QVERIFY(id == ids[0]);
    }
}

//r3 a cancelled build stops early so the next build does not wait for it
void M5StudentTests::worker_stops_cancelled_build_quickly()
{
    DoneCounter counter;
    BuildWorker worker(std::bind(&DoneCounter::notify, &counter));

    //three seconds of simulated build time
    worker.submit(make_workspace(6), 500);
    QThread::msleep(100);
    worker.cancel();

    QElapsedTimer timer;
    timer.start();
    worker.submit(make_workspace(1), 0);
    QVERIFY(counter.wait_for_count(1, 1500));
    QVERIFY(timer.elapsed() < 1000);
}

//r3 a cancelled build never becomes current so it is never reported
void M5StudentTests::worker_never_reports_cancelled_build()
{
    DoneCounter counter;
    BuildWorker worker(std::bind(&DoneCounter::notify, &counter));

    worker.submit(make_workspace(3), 300);
    QThread::msleep(100);
    worker.cancel();

    QVERIFY(!counter.wait_for_count(1, 1500));
    QVERIFY(worker.take_outcome() == nullptr);
}

//r4 shutdown joins an idle worker
void M5StudentTests::worker_shuts_down_when_idle()
{
    DoneCounter counter;
    BuildWorker worker(std::bind(&DoneCounter::notify, &counter));

    worker.submit(make_workspace(1), 0);
    QVERIFY(counter.wait_for_count(1, 2000));

    //give the worker time to go back to waiting for its next job
    QThread::msleep(100);

    ExitWatchdog watchdog(3000);
    worker.shutdown();
    watchdog.finish();
}

//r4 shutdown stops a running build within about one document
void M5StudentTests::worker_shuts_down_mid_build()
{
    DoneCounter counter;
    BuildWorker worker(std::bind(&DoneCounter::notify, &counter));

    worker.submit(make_workspace(6), 500);
    QThread::msleep(100);

    ExitWatchdog watchdog(3000);
    QElapsedTimer timer;
    timer.start();
    worker.shutdown();
    watchdog.finish();

    QVERIFY(timer.elapsed() < 500);
    QCOMPARE(counter.count(), 0);
}

//r2 the gui thread hands a request over without waiting for the build
void M5StudentTests::build_does_not_block_gui()
{
    MainWindow window;
    add_all_files(window);
    window.set_build_delay_ms(500);

    const qint64 click_ms = click_build(window);
    QVERIFY(click_ms >= 0);
    QVERIFY(click_ms < 500);
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Building")));
}

//r5 not built moves to building and then to current and search follows the state
void M5StudentTests::build_becomes_current_and_enables_search()
{
    MainWindow window;
    QVERIFY(window.add_document_file(files_[0]));

    QLineEdit* query_edit = find_query_edit(window);
    QPushButton* search_button = find_button(window, QStringLiteral("Search"));
    QVERIFY(query_edit != nullptr);
    QVERIFY(search_button != nullptr);
    query_edit->setText(QStringLiteral("retrieval"));
    QVERIFY(!search_button->isEnabled());

    window.set_build_delay_ms(200);
    click_build(window);
    QVERIFY(!search_button->isEnabled());

    QTRY_VERIFY_WITH_TIMEOUT(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("chunks")), 3000);
    QVERIFY(search_button->isEnabled());

    QVERIFY(window.add_document_file(files_[1]));
    QVERIFY(!search_button->isEnabled());
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Not built")));
}

//r3 reset during a long build cancels it and the next build does not wait
void M5StudentTests::reset_cancels_running_build()
{
    MainWindow window;
    add_all_files(window);
    window.set_build_delay_ms(500);
    click_build(window);
    QTest::qWait(100);

    QPushButton* reset_button = find_button(window, QStringLiteral("Reset"));
    QVERIFY(reset_button != nullptr);
    QTest::mouseClick(reset_button, Qt::LeftButton);
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Not built")));

    window.set_build_delay_ms(0);
    QVERIFY(window.add_document_file(files_[0]));
    QElapsedTimer timer;
    timer.start();
    click_build(window);
    QTRY_VERIFY_WITH_TIMEOUT(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("chunks")), 1500);
    QVERIFY(timer.elapsed() < 1000);
}

//r3 a new build request while building cancels the old build and starts a fresh one
void M5StudentTests::second_build_request_replaces_running_build()
{
    MainWindow window;
    add_all_files(window);
    window.set_build_delay_ms(500);
    click_build(window);
    QTest::qWait(100);

    window.set_build_delay_ms(0);
    QElapsedTimer timer;
    timer.start();
    click_build(window);
    QTRY_VERIFY_WITH_TIMEOUT(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("chunks")), 1500);
    QVERIFY(timer.elapsed() < 1000);
}

//r3 a workspace change during a build cancels it and the old result never becomes current
void M5StudentTests::workspace_change_cancels_running_build()
{
    MainWindow window;
    add_all_files(window);

    QLineEdit* query_edit = find_query_edit(window);
    QPushButton* search_button = find_button(window, QStringLiteral("Search"));
    QVERIFY(query_edit != nullptr);
    QVERIFY(search_button != nullptr);
    query_edit->setText(QStringLiteral("retrieval"));

    window.set_build_delay_ms(300);
    click_build(window);
    QTest::qWait(100);

    QVERIFY(window.add_document_file(files_[0]));
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Not built")));

    //longer than the whole build would have taken
    QTest::qWait(2200);
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Not built")));
    QVERIFY(!search_button->isEnabled());
}

//r2 a build that finished but is not yet handled is dropped when the workspace changes
void M5StudentTests::finished_build_is_dropped_after_workspace_change()
{
    MainWindow window;
    QVERIFY(window.add_document_file(files_[0]));

    QLineEdit* query_edit = find_query_edit(window);
    QPushButton* search_button = find_button(window, QStringLiteral("Search"));
    QVERIFY(query_edit != nullptr);
    QVERIFY(search_button != nullptr);
    query_edit->setText(QStringLiteral("retrieval"));

    window.set_build_delay_ms(0);
    click_build(window);
    QVERIFY(window.add_document_file(files_[1]));

    QTest::qWait(300);
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Not built")));
    QVERIFY(!search_button->isEnabled());
}

//a failed build returns to not built and the user can build again
void M5StudentTests::failed_build_returns_to_not_built()
{
    MainWindow window;
    QVERIFY(window.add_document_file(files_[0]));

    //the same file twice gives a duplicate document id
    QVERIFY(window.add_document_file(files_[0]));

    click_build(window);
    QTRY_VERIFY_WITH_TIMEOUT(find_label_text(window, QStringLiteral("Current status:")).contains(QStringLiteral("failed")), 2000);
    QVERIFY(find_label_text(window, QStringLiteral("Corpus:")).contains(QStringLiteral("Not built")));

    QPushButton* build_button = find_button(window, QStringLiteral("Build Corpus"));
    QVERIFY(build_button != nullptr);
    QVERIFY(build_button->isEnabled());
}

//r4 destroying the window during a build stops the worker within about one document
void M5StudentTests::exit_stops_running_build()
{
    auto window = std::make_unique<MainWindow>();
    add_all_files(*window);
    window->set_build_delay_ms(500);
    click_build(*window);
    QTest::qWait(100);

    ExitWatchdog watchdog(3000);
    QElapsedTimer timer;
    timer.start();
    window.reset();
    watchdog.finish();
    QVERIFY(timer.elapsed() < 500);
}

//r4 destroying the window when no build is running does not hang
void M5StudentTests::exit_works_when_no_build_is_running()
{
    auto window = std::make_unique<MainWindow>();
    QVERIFY(window->add_document_file(files_[0]));
    window->set_build_delay_ms(0);
    click_build(*window);
    QTRY_VERIFY_WITH_TIMEOUT(find_label_text(*window, QStringLiteral("Corpus:")).contains(QStringLiteral("chunks")), 2000);

    //give the worker time to go back to waiting for its next job
    QTest::qWait(100);

    ExitWatchdog watchdog(3000);
    window.reset();
    watchdog.finish();
}

QTEST_MAIN(M5StudentTests)
#include "student_tests.moc"
