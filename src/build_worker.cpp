#include "aiws/gui/build_worker.hpp"

#include <chrono>
#include <exception>
#include <utility>
#include <iostream>

namespace aiws::gui {

namespace {

//a build that produced a corpus
class SucceededOutcome final : public BuildOutcome
{
public:
    explicit SucceededOutcome(ProcessingCore&& core) : core_(std::move(core)) {}

    void deliver(BuildSink& sink) override
    {
        sink.build_succeeded(std::move(core_));
    }

private:
    ProcessingCore core_;
};

//a build that threw
class FailedOutcome final : public BuildOutcome
{
public:
    void deliver(BuildSink& sink) override
    {
        sink.build_failed();
    }
};

}  // namespace

//starts the single worker thread
BuildWorker::BuildWorker(DoneCallback on_done)
    : on_done_(std::move(on_done)), thread_(&BuildWorker::run, this)
{
}

//stops and joins the worker thread
BuildWorker::~BuildWorker()
{
    shutdown();
}

//hands a snapshot to the worker without waiting
void BuildWorker::submit(Workspace snapshot, int delay_ms)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = Job{std::move(snapshot), delay_ms};
        outcome_.reset();
    }
    cv_.notify_one();
}

//drops the pending request and wakes the running build so it stops now
void BuildWorker::cancel()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.reset();
        outcome_.reset();
#ifdef AIWS_FAULT_NO_CANCEL
        std::cerr << "[fault] NO_CANCEL active" << std::endl;
#else
        std::cerr << "[normal] cancel active" << std::endl;
#endif
        if (running_)
        {
#ifndef AIWS_FAULT_NO_CANCEL
            cancel_requested_ = true;
#endif
        }
    }
    cv_.notify_all();
}

//moves out the finished outcome or returns null when there is none
std::unique_ptr<BuildOutcome> BuildWorker::take_outcome()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::move(outcome_);
}

//stops the worker at any moment and joins it
void BuildWorker::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
#ifndef AIWS_FAULT_SILENT_SHUTDOWN
    cv_.notify_all();
#endif
    if (thread_.joinable())
    {
        thread_.join();
    }
}

//the worker loop that waits for a job builds it and reports it
void BuildWorker::run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (true)
    {
        //sleep until there is a job or a stop request
        cv_.wait(lock, [this] { return stop_ || pending_.has_value(); });
        if (stop_)
        {
            return;
        }

        Job job = std::move(*pending_);
        pending_.reset();
        running_ = true;
        cancel_requested_ = false;

        //simulate build time on the same cv so cancel and stop wake it at once
        bool wanted = true;
        const long long total_delay = static_cast<long long>(job.delay_ms) * static_cast<long long>(job.snapshot.documents().size());
        if (total_delay > 0)
        {
            wanted = !cv_.wait_for(lock, std::chrono::milliseconds(total_delay), [this] { return stop_ || cancel_requested_; });
        }

        //build on a local core so no backend data is shared with the gui thread
        if (wanted)
        {
            lock.unlock();
            std::unique_ptr<BuildOutcome> outcome;
            try
            {
                ProcessingCore core;
                core.rebuild(job.snapshot);
                outcome = std::make_unique<SucceededOutcome>(std::move(core));
            }
            catch (const std::exception&)
            {
                outcome = std::make_unique<FailedOutcome>();
            }
            lock.lock();

            wanted = !stop_ && !cancel_requested_;
            if (wanted)
            {
                outcome_ = std::move(outcome);
            }
        }

        running_ = false;

        //only a build that is still wanted is reported
        if (wanted)
        {
            lock.unlock();
            on_done_();
            lock.lock();
        }
    }
}

}  // namespace aiws::gui
