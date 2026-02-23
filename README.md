Overview This project consists of two distinct chat communication systems developed in C++ for the Windows environment.

TCP/IP Socket Chat: A reliable, connection-oriented chat environment using the Windows Sockets 2 (Winsock2) library. It follows a standard Client-Server model where a central server manages connections on port 60000 and broadcasts messages to connected clients. +2

Shared Memory (SHM) Chat: A local Inter-Process Communication (IPC) system allowing separate processes on the same machine to communicate via a shared block of RAM. It relies on CreateFileMapping and utilizes Semaphores to ensure mutual exclusion and prevent race conditions. +2

Both systems feature multithreading to allow simultaneous typing and receiving, and both come with standard console versions as well as custom Graphical User Interface (GUI) clients. +1

Compilation and Execution Instructions

The Server (win_server.cpp)
Compile: g++ win_server.cpp -o server.exe -lws2_32

Run: ./server.exe

The Client (win_client.cpp)
Compile: g++ win_client.cpp-o client.exe -lws2_32

Run: .\client.exe

The Client with GUI (win_socket_chat_gui.cpp)
Compile: g++ win_socket_chat_gui.cpp-o gui_socket.exe -lws2_32-mwindows

Run: .\gui_socket.exe Alice (or enter any username)

The Shared Memory Console (win_shm_chat.cpp)
Compile: g++ win_shm_chat.cpp-o shm_chat.exe -lws2_32

Run: .\shm_chat.exe User1 (Run again in a new terminal with User2)

The Shared Memory GUI (win_shm_chat_gui.cpp)
Compile: g++ win_shm_chat_gui.cpp-o gui_shm.exe -mwindows

Run: .\gui_shm.exe User1 (Run again in a new terminal with User2) +1

(Note: If an error occurs while compiling, ensure your compiler path is correct, e.g., using the full path to g++.exe)
