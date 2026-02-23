// --- IMPORTS ---
#include <iostream>     // Allows us to print to the console (std::cout)
#include <vector>       // Allows us to use a dynamic list to store connected clients
#include <string>       // Allows us to use text strings
#include <thread>       // Allows the program to do multiple things at once (Multithreading)
#include <mutex>        // "Mutual Exclusion" - prevents two threads from messing up data at the same time
#include <winsock2.h>   // The main Windows library for networking (Sockets)
#include <algorithm>    // Helper functions to find/remove items from lists

// --- LINKING ---
// This tells the compiler to grab the "ws2_32.lib" system file. 
// Without this, Windows won't understand any networking commands.
#pragma comment(lib, "ws2_32.lib")

// --- CONSTANTS ---
#define PORT 60000       // We will listen on port 8080. Like a specific door number on a building.

// --- GLOBAL VARIABLES ---
std::vector<SOCKET> clients; // A list that holds the ID cards (sockets) of everyone connected
std::mutex clients_mutex;    // A lock. Only one thread can touch the 'clients' list when this is locked.

// --- FUNCTION: BROADCAST ---
// This function sends a message to everyone EXCEPT the person who sent it.
void broadcast(std::string message, SOCKET sender_socket) {
    // Lock the door! We are reading the client list, so nobody else should add/remove clients right now.
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    // Loop through every client in our list
    for (SOCKET client : clients) {
        // If this client is NOT the sender...
        if (client != sender_socket) {
            // Send the message to them!
            // .c_str() turns the C++ string into a raw style C-string that the network understands.
            send(client, message.c_str(), message.size(), 0);
        }
    }
    // The lock is automatically unlocked here when the function finishes.
}

// --- FUNCTION: HANDLE CLIENT ---
// This function runs on a separate thread for EACH user.
// It listens for their messages forever until they disconnect.
void handle_client(SOCKET client_socket) {
    char buffer[1024]; // A temporary container to hold incoming messages (max 1024 characters)
    
    while (true) {
        // Clean the buffer (fill it with zeros) so old messages don't get mixed in
        ZeroMemory(buffer, 1024);
        
        // recv() waits here until data arrives. It is "blocking".
        // It returns the number of bytes received.
        int bytes_received = recv(client_socket, buffer, 1024, 0);
        
        // If bytes_received is 0 or less, it means the user closed the window or lost internet.
        if (bytes_received <= 0) {
            // Close the connection properly
            closesocket(client_socket);
            
            // Lock the list again because we are about to remove someone
            std::lock_guard<std::mutex> lock(clients_mutex);
            
            // Find this client in the list and remove them
            clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
            
            std::cout << "Client disconnected." << std::endl;
            break; // Break the loop to stop this thread
        }

        // If we got a message, convert it to a string
        std::string msg(buffer);
        
        // Send this message to everyone else
        broadcast(msg, client_socket);
    }
}

// --- MAIN FUNCTION ---
// This is where the program starts.
int main() {
    // 1. STARTUP WINSOCK
    WSADATA wsaData; // Structure to hold Windows socket data
    // WSAStartup turns on the networking capability in Windows.
    // MAKEWORD(2, 2) asks for version 2.2.
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n"; // Print error if it fails
        return 1; // Exit program with error code
    }

    // 2. CREATE THE SERVER SOCKET
    SOCKET server_socket;
    // AF_INET = IPv4 (Standard internet address)
    // SOCK_STREAM = TCP (Reliable connection, like a phone call)
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    // 3. SETUP ADDRESS STRUCTURE
    sockaddr_in address;
    address.sin_family = AF_INET; // Use IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address on this computer
    address.sin_port = htons(PORT); // Set the port (8080). htons converts numbers to "Network Byte Order"

    // 4. BIND
    // Assign the IP and Port to our socket.
    if (bind(server_socket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        return 1;
    }

    // 5. LISTEN
    // Start listening for incoming calls. '3' is the backlog (how many people can wait on hold).
    if (listen(server_socket, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // 6. ACCEPT LOOP
    // This loop runs forever, accepting new people.
    while (true) {
        SOCKET new_socket;
        int addrlen = sizeof(address);
        
        // accept() stops and waits here until someone tries to connect.
        // When they do, it returns a NEW socket just for that person.
        if ((new_socket = accept(server_socket, (sockaddr*)&address, &addrlen)) == INVALID_SOCKET) {
            continue; // If connection failed, just try again (continue loop)
        }

        // Add the new person to our list (Thread safe!)
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(new_socket);
        }
        
        // Create a new thread (a worker) to handle this specific person.
        // We pass the function 'handle_client' and the user's socket.
        std::thread t(handle_client, new_socket);
        
        // detach() lets the thread run on its own in the background. 
        // We don't need to wait for it to finish.
        t.detach(); 
    }

    // Cleanup (Note: Code never actually reaches here because the while(true) loop is infinite)
    closesocket(server_socket);
    WSACleanup(); // Turn off Winsock
    return 0;
}