#include <iostream>
#include <string>
#include <windows.h> // Required for Windows Shared Memory functions
#include <thread>

// Unique names so both programs find the same memory location
#define SHM_NAME "Local\\MyChatMemory" 
#define SEM_NAME "Local\\MyChatSemaphore"
#define SHM_SIZE 1024

// This struct defines what the shared memory looks like
struct ChatData {
    char message[256];
    char sender[50];
    int message_id; // Acts like a version number. Increases when new message arrives.
};

// Thread to watch for new messages
void receiver_thread(ChatData* shared_mem, std::string my_name) {
    int last_seen_id = shared_mem->message_id;
    while (true) {
        // If the ID in memory is higher than what we last saw, it's a new message!
        if (shared_mem->message_id > last_seen_id) {
            // Only print if it wasn't ME who sent it
            if (std::string(shared_mem->sender) != my_name) {
                std::cout << "\r[" << shared_mem->sender << "]: " << shared_mem->message << "\n> " << std::flush;
            }
            last_seen_id = shared_mem->message_id; // Update our tracker
        }
        Sleep(100); // Wait 0.1 seconds before checking again (saves CPU)
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: win_shm_chat.exe <username>\n";
        return 1;
    }
    std::string username = argv[1];

    // 1. Create/Open Shared Memory in RAM
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    
    // 2. Map it so we can access it like a variable
    ChatData* pBuf = (ChatData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    
    // 3. Create Semaphore (Lock) to prevent writing at exact same time
    HANDLE hSemaphore = CreateSemaphore(NULL, 1, 1, SEM_NAME);

    system("cls"); 
    std::cout << "--- SHARED MEMORY CHAT (" << username << ") ---\n> ";

    // Start background listener
    std::thread t(receiver_thread, pBuf, username);
    t.detach();

    // Main Input Loop
    while (true) {
        std::string msg;
        std::getline(std::cin, msg);
        if (msg == "exit") break;

        // LOCK: Wait for permission to write
        WaitForSingleObject(hSemaphore, INFINITE);
        
        // WRITE data to RAM
        strcpy(pBuf->message, msg.c_str());
        strcpy(pBuf->sender, username.c_str());
        pBuf->message_id++; // Increment ID
        
        // UNLOCK: Give permission back
        ReleaseSemaphore(hSemaphore, 1, NULL);

        std::cout << "> ";
    }
    return 0;
}