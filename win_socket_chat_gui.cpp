#define UNICODE                // Use Unicode characters for Windows APIs
#define _UNICODE               // Define Unicode for C runtime

#include <winsock2.h>   // <--- MOVE THIS TO THE TOP!
#include <ws2tcpip.h>   // <--- Keep this with it
#include <windows.h>           // Win32 core functions
#include <string>              // std::string and std::wstring
#include <thread>              // For background receiving thread
#include <mutex>               // For thread-safe access
#include <atomic>              // For atomic flags
#include <winsock2.h>          // Winsock socket API
#include <ws2tcpip.h>          // IP helper functions

#pragma comment(lib, "Ws2_32.lib")  // Link Winsock library

// GUI control IDs
#define ID_SEND_BUTTON 1001    // Send button ID
#define ID_INPUT_BOX   1002    // Input box ID
#define ID_CHAT_LOG    1003    // Chat log ID

// Global GUI handles
HWND g_hChatLog = NULL;        // Handle to chat log
HWND g_hInputBox = NULL;       // Handle to input box
HBRUSH g_hBackgroundBrush = NULL; // Background brush

std::string g_username;        // User name
std::mutex g_chat_mutex;       // Mutex to protect chat log

// Socket data
SOCKET g_sock = INVALID_SOCKET; // Client socket
std::atomic<bool> g_running(false); // Flag for running thread
std::thread g_recv_thread;     // Thread for receiving messages

// Append text to chat log (thread-safe)
void AppendToChatLog(const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_chat_mutex); // Lock mutex

    if (!g_hChatLog) return;                       // Exit if chat log not ready

    std::wstring wtext(text.begin(), text.end());  // Convert string to wide string

    int len = GetWindowTextLengthW(g_hChatLog);    // Get current text length
    SendMessageW(g_hChatLog, EM_SETSEL, len, len); // Move cursor to end
    SendMessageW(g_hChatLog, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str()); // Insert text
    SendMessageW(g_hChatLog, WM_VSCROLL, SB_BOTTOM, 0); // Scroll to bottom
}

// Background receive thread
void RecvLoop()
{
    g_running = true;                  // Set running flag
    char buf[1024];                    // Buffer for received data

    while (g_running)
    {
        ZeroMemory(buf, sizeof(buf));  // Clear buffer
        int bytes = recv(g_sock, buf, sizeof(buf) - 1, 0); // Receive data

        if (bytes > 0)
        {
            buf[bytes] = '\0';         // Null-terminate
            AppendToChatLog(std::string("[Peer]: ") + buf + "\r\n"); // Show peer message
        }
        else
        {
            AppendToChatLog("[System]: Disconnected.\r\n"); // Show disconnect
            g_running = false;          // Stop loop
            break;
        }
    }
}

// Connect to server 127.0.0.1:8080
bool ConnectToServer()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)     // Initialize Winsock
        return false;

    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Create TCP socket
    if (g_sock == INVALID_SOCKET)
        return false;

    sockaddr_in addr;
    addr.sin_family = AF_INET;                      // IPv4
    addr.sin_port = htons(60000);                    // Port 8080
    inet_pton(AF_INET, "10.223.0.249", &addr.sin_addr); // IP 127.0.0.1

// if andrew server (10.223.0.249)
// if Bevnoty server (10.223.0.8)

    if (connect(g_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) // Connect
        return false;

    g_recv_thread = std::thread(RecvLoop);         // Start receive thread
    g_recv_thread.detach();                        // Detach thread

    return true;                                   // Connected
}

// Send message action
void SendMessageAction()
{
    if (g_sock == INVALID_SOCKET) return;          // Exit if socket invalid

    int len = GetWindowTextLengthW(g_hInputBox);  // Get input length
    if (len == 0) return;                          // Exit if empty

    std::wstring wmsg(len, L'\0');                // Prepare buffer
    GetWindowTextW(g_hInputBox, &wmsg[0], len + 1); // Get input text

    std::string msg(wmsg.begin(), wmsg.end());    // Convert to string
    std::string full = "[" + g_username + "]: " + msg; // Format message

    send(g_sock, full.c_str(), (int)full.size(), 0); // Send to server

    AppendToChatLog("[You]: " + msg + "\r\n");    // Show in chat log

    SetWindowTextW(g_hInputBox, L"");             // Clear input box
}

// Cleanup socket
void CleanupSocket()
{
    g_running = false;                             // Stop thread
    if (g_sock != INVALID_SOCKET)
    {
        shutdown(g_sock, SD_BOTH);                // Shutdown socket
        closesocket(g_sock);                       // Close socket
        g_sock = INVALID_SOCKET;                  // Reset
    }
    WSACleanup();                                  // Cleanup Winsock
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        std::wstring title = 
            L"--- SOCKET CHAT (" 
            + std::wstring(g_username.begin(), g_username.end()) 
            + L") ---";                         // Title with username

        CreateWindowW(L"STATIC", title.c_str(),
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 5, 480, 20,
            hwnd, NULL, NULL, NULL);               // Show title

        g_hChatLog = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_READONLY | ES_MULTILINE,
            10, 30, 480, 230,
            hwnd, (HMENU)ID_CHAT_LOG, NULL, NULL); // Chat log

        g_hInputBox = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 270, 400, 25,
            hwnd, (HMENU)ID_INPUT_BOX, NULL, NULL); // Input box

        CreateWindowW(L"BUTTON", L"Send",
            WS_CHILD | WS_VISIBLE,
            420, 270, 70, 25,
            hwnd, (HMENU)ID_SEND_BUTTON, NULL, NULL); // Send button

        g_hBackgroundBrush = CreateSolidBrush(RGB(224,255,235)); // Background color

        AppendToChatLog("[System]: Connecting...\r\n"); // Show connecting

        if (ConnectToServer())
            AppendToChatLog("[System]: Connected.\r\n"); // Show connected
        else
            AppendToChatLog("[System]: Connection failed.\r\n"); // Show fail

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_SEND_BUTTON)
            SendMessageAction();                       // Send button clicked
        return 0;

    case WM_DESTROY:
        if (g_hBackgroundBrush) DeleteObject(g_hBackgroundBrush); // Delete brush
        CleanupSocket();                               // Close socket
        PostQuitMessage(0);                            // Quit app
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam); // Default handling
}

// Main entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc); // Get command line

    if (argc < 2)
        g_username = "User";                             // Default username
    else
    {
        std::wstring w(argv[1]);
        g_username = std::string(w.begin(), w.end());   // Use provided username
    }

    LocalFree(argv);                                     // Free argv

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;                            // Set window procedure
    wc.hInstance = hInst;                                // Instance handle
    wc.lpszClassName = L"SocketChatGUI";                // Window class name

    RegisterClassW(&wc);                                 // Register window class

    HWND hwnd = CreateWindowW(
        L"SocketChatGUI",
        L"Socket Chat",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT,
        520, 350,
        NULL, NULL, hInst, NULL);                        // Create main window

    ShowWindow(hwnd, nCmdShow);                          // Show window

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))               // Message loop
    {
        TranslateMessage(&msg);                          // Translate messages
        DispatchMessageW(&msg);                          // Dispatch messages
    }

    return 0;                                            // Exit program
}


// to run it put the path and the command from the read me file
// example:
// "C:\Users\mamdo\w64devkit\bin\g++.exe" win_server.cpp -o server.exe -lws2_32