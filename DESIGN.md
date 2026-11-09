# M4 Decision Record

## 1. Threads and Ownership 

The application has a GUI thread, a build worker thread, and an IPC server thread. The GUI thread contains the Qt event loop and holds all the visual elements. The build thread is a joinable std::thread that gets created whenever the corpus build process starts. The IPC thread is a joinable std::thread managed by IpcServer and is used for listening to remote add requests.

According to constraint 5, there is only one mutable element accessible from multiple threads, which is the atomic integer build_generation_. It gets written to by the GUI thread to indicate that either a new build has begun or an invalidation has happened. The build thread reads the build generation number to mark the results of its work. The rest of the data elements, such as corpus_current_ , the ProcessingCore backend, and all the widgets, are accessed exclusively by the GUI thread.

## 2. Build Validity

No generative AI was used in the creation of this implementation. 
The validity of a complete build can only be determined by checking that the generation of the build is equal to the value of the variable build_generation_. As soon as the user clicks Build Corpus, the generation is increased and this new number is sent to the worker. In case the user creates another document through the GUI or Remote IPC connection, or when the user clicks Reset button while building the corpus, the generation is increased once again. Once the worker is done, the result is received through QMetaObject::invokeMethod, and then compared to the generation of MainWindow::on_build_finished.

## 3. Changes Since the Checkpoint

For the first checkpoint design, the choice fell on std::async and std::future. For the second checkpoint, std::thread and QMetaObject::invokeMethod are used. The second was adopted to avoid the problem of the GUI thread being blocked by future while exiting or resetting the application during building process. Instead, the atomic flag cancel_requested_ is now checked in 50ms steps.

## 4. CR-1 Impact

CR-1 implemented a new class - IpcServer - in src/ipc_server.cpp and runs it in its own std::thread. It listens to POSIX Unix domain socket and processes incoming commands. If the received command is valid "add <path>", the server calls MainWindow::add_document_file via QMetaObject::invokeMethod with Qt::BlockingQueuedConnection. That way we avoid problems of GUI update and the problem of accessing the same data from build thread and IPC thread. The remaining part of the application (all GUI widgets, all backend algorithms) has not been touched as the IPC layer is completely separated from it.

## 5. CR-1 Byte-Level Message Format

The IPC message format is designed to be simple and robust against fragmentation. It uses plain UTF-8 text terminated by a single newline (\n, ASCII 10).

Request (from aiws_send to the application):
The message is the ASCII string add  (4 bytes) followed by the absolute or relative file path (UTF-8) followed by a newline.
Example: add /home/user/docs/file.txt\n

Response (from the application to aiws_send):
Exactly 1 byte.
'0' (ASCII 48) indicates the application accepted the request and the file is readable.
'1' (ASCII 49) indicates the application rejected the request (e.g., file unreadable).
If no byte is received, aiws_send assumes a communication failure (exit code 2).

The server reads bytes in chunks and accumulates them until it finds the \n terminator, satisfying requirement E3.

## 6. Fault Switch Locations

The AIWS_FAULT_ACCEPT_STALE is located in src/main_window.cpp in the MainWindow::on_build_finished function. If defined, this switch allows the program to skip the generation checking and accept the stale build output. This condition is tested using testR2CorrectnessStaleResult in tests/student_tests.cpp, which makes sure that Search button stays disabled after the stale build.

The AIWS_FAULT_LOSE_FAILURE is located in src/main_window.cpp in the MainWindow::on_build_finished function. If defined, it makes the success flag true, making the build failure invisible for the GUI. This condition is tested using testR3Failure, which checks whether the status label contains "failed" or "Not built".

## 7. AI-Use Disclosure

No generative AI was used in the creation of this implementation. 

## 8. Pair programming log

Partner: Daniel Kim

Session 1 (Partner's Code): November 8, 2026, 8:53PM - 9:55PM. Worked on tools/aiws_send.cpp. Daniel was the Driver and I was the Navigator recommending him to use read() in a loop in order to handle partial replies and to use exactly specified exit codes (0, 1, 2). We switched roles after about 30 minutes, and I started working on the argument parsing logic in tools/main.cpp to deal with the --socket path flag and extract the "add <file>" command. I also implemented the UTF-8 path conversion and made sure the tool returned exit code 2 if the socket failed within 2 seconds.

Session 2 (My Code): November 8, 2026, 10:05PM - 11:11PM. Worked on CR-1 IPC server and socket logic. I was the Driver for the first hour doing the implementation of IpcServer class and POSIX sockets. Daniel was the Navigator and he recommended using Qt::BlockingQueuedConnection in order to transfer the received file path back to the GUI thread safely. We switched roles after about 30 minutes, and Daniel became the Driver doing the read_request buffering loop, whereas I became the Navigator and noticed that we have to implement handling partial reads. The rest of the session was devoted to fixing the hang during the accept() when shutting down the code and implementing the shutdown() in order to unblock the thread.

Navigator Contribution: I suggested the use of another main.cpp file within tools/ in order to allow for the code to be modular and for organization to be easier

Contribution by Partner: Daniel recommended the use of \n as the terminator for the IPC message format; this made handling of the fragmented messages in read_request easy.

Best Contribution: The best contribution was the one where we used a second screen to go through the POSIX documentation of accept() while trying to debug the shutdown of sockets. We found out that we need to call shutdown() to unblock threads stuck in accept().

