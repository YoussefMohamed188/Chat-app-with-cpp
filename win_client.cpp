#include <iostream>     // For printing
#include <string>       // For text
#include <thread>       // For running receive and send at same time
#include <winsock2.h>   // Windows Networking
#include <ws2tcpip.h>   // Helper for converting IP addresses

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define PORT 60000

// Thread function: Listens for incoming messages from server
void listen_for_messages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        ZeroMemory(buffer, 1024); // Clear buffer
        
        // Wait to receive data
        int bytes_received = recv(sock, buffer, 1024, 0);
        
        // If connection lost
        if (bytes_received <= 0) {
            std::cout << "\nDisconnected from server.\n";
            break;
        }
        // Print the message. \r moves cursor to start of line to look pretty.
        std::cout << "\r" << buffer << "\n> " << std::flush;
    }
}

int main() {
    // 1. Start Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr;
    std::string username;

    std::cout << "Enter Username: ";
    std::getline(std::cin, username); // Get username from keyboard
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert "127.0.0.1" (Localhost) to binary format
    inet_pton(AF_INET, "10.223.0.249", &serv_addr.sin_addr);
// if andrew server (10.223.0.249)
// if Bevnoty server (10.223.0.8)
    // 2. Connect to the Server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "\nConnection Failed (Is Server Running?)\n";
        return -1;
    }

    system("cls"); // Clear the terminal screen
    std::cout << "--- CHAT ROOM (" << username << ") ---\n> ";

    // 3. Start the listener thread (so we can receive while typing)
    std::thread t(listen_for_messages, sock);
    t.detach();

    // 4. Main Loop: Reading Keyboard Input
    while (true) {
        std::string msg;
        std::getline(std::cin, msg); // Wait for user to type line
        
        if (msg == "exit") break; // Allow user to quit

        std::string full_msg = "[" + username + "]: " + msg;
        
        // Send message to server
        send(sock, full_msg.c_str(), full_msg.length(), 0);
        
        std::cout << "> "; // Print the prompt again
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}