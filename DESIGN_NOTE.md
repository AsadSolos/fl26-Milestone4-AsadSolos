# M4 Design Note

## 1. Components and Threads

The application consists of a GUI thread and a worker thread. The GUI thread uses a standard single-threaded Qt event loop and has all visual components. These are MainWindow (which acts as coordinator, owns the backend Workspace and ProcessingCore), four GUI widgets WorkspaceWidget, SearchWidget, TermInspectorWidget, ContextWidget) and SearchResultModel. The worker thread is a single joinable std::thread that is started on the button click of Build Corpus. It does actual backend processing by invoking processing_core_.rebuild() on the snapshot of the workspace, and applies artificial delay by std::this_thread::sleep_for().

As per constraint 5, the only mutable data object accessible from more than one thread is the atomic integer variable build_generation_. The GUI thread writes to it to indicate either a new build request or invalidation of existing corpus, while the worker thread reads it to mark its result. Everything else, including corpus_current_, backend state and all widgets, are accessed in a single GUI thread only. The worker gets a copy of the Workspace by value, thus backend data structures are not shared between threads.

## 2. Corpus States and Transitions

The corpus can be in one of three states: Not Built, Building, and Current. It begins in the Not Built state. Upon clicking on either Add Document or Reset, the state does not change or is set to Not Built. On clicking on Build Corpus, the state is set to Building. This locks out Search, Inspect, and Build Context buttons to avoid executing any outdated queries. Once the worker thread finishes, the signal is passed via the queue to transition into the Current state. Invalidations during builds transition the state back to Not Built. If a build fails, the state also returns to Not Built and an error is displayed.

## 3. Threatening Event Sequences

The main risk to the R2 correctness invariant is the case when the user makes changes to the workspace while a build is still ongoing. For instance, the user clicks Build Corpus (triggering background thread) and subsequently clicks on Add Document or Reset. In this scenario, the GUI thread updates the workspace and increments build_generation_. When the worker thread finishes processing the old snapshot of the workspace, it sends a message containing the old generation number. MainWindow catches the signal in the queue, checks whether the generation number is the same as build_generation_, and drops it as a result.

The second risk arises from failure states. In the event that the build fails, say because of repeated document IDs, the worker will notify the application of the problem. If the system does not account for such failures (as assumed in the use of AIWS_FAULT_LOSE_FAILURE), the application gets stuck in the Building state. In order to fulfill R3 requirements, the worker captures the exception and notifies the GUI of the failure state. The GUI resets the Building state and presents an error status, and the Build Corpus button becomes active. As far as the R4 life cycle is concerned, if the user clicks on the Exit button when building is ongoing, the MainWindow class sets the atomic cancellation flag and joins the worker thread.

## 4. The Ownership Chain of the Generated Corpus

During a build process, the MainWindow copies the current workspace to its local snapshot and sends it to the worker thread. The worker gives the snapshot to the ProcessingCore, which stores the corpus itself and the index of corpus chunks in a private member using std::unique_ptr and standard value members. When the worker finishes working, it builds the BuildResult that consists of the success flag, the number of chunks, and the generation tag. Then this result is sent to the GUI thread via QMetaObject::invokeMethod. After that, the worker thread is terminated. In turn, the corpus itself belongs to the ProcessingCore member inside MainWindow. As for the local snapshot, it is automatically cleaned up when the thread is finished.

## 5. The Rejected Solution

I took into account an idea of using std::async and std::future along with a QTimer that would poll the future periodically. This solution was refused because sometimes retrieving the future blocks the thread and makes the Exit and Reset process more complicated. Moreover, it makes no sense to use the timer because there will be some delay between polling.
