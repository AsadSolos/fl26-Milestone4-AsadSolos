#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "aiws/processing_core.hpp"
#include "aiws/workspace.hpp"

namespace aiws::gui {

//receives the outcome of a build on the gui thread
class BuildSink
{
public:
    virtual ~BuildSink() = default;
    virtual void build_succeeded(ProcessingCore&& core) = 0;
    virtual void build_failed() = 0;
};

//a finished build that knows its own outcome and reports it to a sink
class BuildOutcome
{
public:
    virtual ~BuildOutcome() = default;
    virtual void deliver(BuildSink& sink) = 0;
};

//owns the one persistent build thread that lives from construction until shutdown
class BuildWorker
{
public:
    //callback run on the worker thread when a wanted build has finished
    using DoneCallback = std::function<void()>;

    //starts the single worker thread
    explicit BuildWorker(DoneCallback on_done);

    //stops and joins the worker thread
    ~BuildWorker();

    BuildWorker(const BuildWorker&) = delete;
    BuildWorker& operator=(const BuildWorker&) = delete;

    //hands a snapshot to the worker without waiting
    void submit(Workspace snapshot, int delay_ms);

    //drops the pending request and wakes the running build so it stops now
    void cancel();

    //moves out the finished outcome or returns null when there is none
    std::unique_ptr<BuildOutcome> take_outcome();

    //stops the worker at any moment and joins it
    void shutdown();

private:
    //one request waiting for the worker
    struct Job
    {
        Workspace snapshot;
        int delay_ms{0};
    };

    //the worker loop that waits for a job builds it and reports it
    void run();

    //set once at construction and never changed so it needs no lock
    DoneCallback on_done_;

    //everything below is shared between the gui thread and the worker and is guarded by mutex_
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> pending_;
    std::unique_ptr<BuildOutcome> outcome_;
    bool running_{false};
    bool cancel_requested_{false};
    bool stop_{false};

    //declared last so every member above exists before the thread starts
    std::thread thread_;
};

}  // namespace aiws::gui
