# M5 Design Document

## 1. M4 Fixes

The M4 implementation used a plain text protocol for CR-1 where the IPC message was terminated by a newline (`\n`). If a file path contained a literal newline, the server parsed the message incorrectly and failed to add the file. I fixed this by implementing a length-prefix protocol. The sender (`tools/aiws_send.cpp`) now writes a 4-byte unsigned integer (`uint32_t`) containing the exact byte length of the command, followed by the command bytes. The receiver (`src/ipc_server.cpp`) reads the 4 bytes first, resizes the buffer to that exact length, and reads exactly that many bytes. This makes the IPC layer immune to spaces, UTF-8 characters, and newlines. No other M4 bugs were found.

## 2. Threads and Shared Data (Constraint 4)

The application has two threads. The GUI thread runs the Qt event loop, owns all widgets, and holds the `CorpusState` objects. The worker thread is a single persistent `std::thread` owned by the `BuildWorker` class; it exists from the first build until shutdown. 

The shared mutable data between the GUI thread and the worker thread is protected by a `std::mutex` inside `BuildWorker`. This data consists of:
- `pending_`: An optional `Job` struct holding the `Workspace` snapshot and delay.
- `outcome_`: A `std::unique_ptr` to the finished `BuildOutcome`.
- `running_`, `cancel_requested_`, and `stop_`: Booleans guarding the worker's state.

Additionally, the `on_done_` callback is shared, but it is set once in the constructor and never modified, so it is safely accessed without a lock. 

This is the smallest possible set. The actual `ProcessingCore` and the `Workspace` snapshot are never shared; the worker builds a local `ProcessingCore` and moves it back to the GUI thread via the outcome.

## 3. Corpus States

The lifecycle is managed by the State pattern. `CorpusState` is an abstract base class with five virtual event handlers: `handle_build_request`, `handle_workspace_change`, `handle_build_succeeded`, `handle_build_failed`, and `handle_reset`. It also has `allows_processing()`.

- **NotBuiltState:** No valid corpus exists. A build request starts the worker and transitions to Building. A workspace change keeps it here and only refreshes the display. Processing is disabled.
- **BuildingState:** A build is running. A build request cancels the current build and starts a new one. A workspace change or reset cancels the build and transitions to NotBuilt. A successful build transitions to Current. Processing is disabled.
- **CurrentState:** A valid corpus exists. A build request starts a new build and transitions to Building. A workspace change or reset transitions to NotBuilt. Processing is allowed.

Every transition uses `MainWindow::set_state(std::make_unique<NextState>())`, replacing the state object. There are no `switch` statements or if-chains deciding transitions.

## 4. Event Sequence Threatening R1–R4

Consider the user clicking Build while a build is running, then pressing Reset before the new build finishes.
1. User clicks Build. `MainWindow::build_corpus()` calls `current_state_->handle_build_request(*this)`.
2. `BuildingState::handle_build_request` calls `MainWindow::cancel_build()` (which calls `build_worker_->cancel()`), then `MainWindow::start_build()` (which calls `build_worker_->submit()`), and stays in `BuildingState`.
3. The worker wakes from `cv_.wait_for`, sees `cancel_requested_` is true, sets `wanted = false`, and skips the build entirely.
4. User clicks Reset. `MainWindow::reset_application()` calls `current_state_->handle_reset(*this)`.
5. `BuildingState::handle_reset` calls `MainWindow::cancel_build()`, then `MainWindow::restore_startup()`, then `MainWindow::set_state(std::make_unique<NotBuiltState>())`.
6. The worker sees `cancel_requested_`, aborts the build, and does not report an outcome. The GUI safely stays in `NotBuiltState`.

## 5. The Three Design Decisions

1. **Build during a build:** The new request cancels the running build and starts fresh. `BuildingState::handle_build_request` calls `cancel_build()` and then `start_build()`. The user immediately sees a fresh "Building..." status.
2. **Remote add mid-build:** A remote add calls `add_document_file()`, which calls `current_state_->handle_workspace_change(*this)`. In `BuildingState`, this cancels the running build and drops to `NotBuiltState`. The user sees the corpus become invalid.
3. **Shared data:** The only shared mutable state is the `pending_` job, the `outcome_` pointer, and the three state booleans inside `BuildWorker`. They are protected by `mutex_`. The `Workspace` and `ProcessingCore` are never shared. This is the smallest set because nothing else needs to cross the boundary.

## 6. Fault Switch Locations

`AIWS_FAULT_NO_CANCEL` is located in `src/build_worker.cpp` inside `BuildWorker::cancel()`. When defined, it skips setting `cancel_requested_ = true`, so the worker continues building even after a cancel. This is caught by `worker_stops_cancelled_build_quickly`.

`AIWS_FAULT_SILENT_SHUTDOWN` is located in `src/build_worker.cpp` inside `BuildWorker::shutdown()`. When defined, it skips `cv_.notify_all()`. If the worker is idle waiting on the condition variable, it never wakes to see `stop_ = true`, causing a hang on exit. This is caught by `worker_shuts_down_when_idle` and `worker_shuts_down_mid_build` (the `ExitWatchdog` kills the process if it hangs).

## 7. AI-Use Disclosure

AI assistance was used to help write DESIGN.md
