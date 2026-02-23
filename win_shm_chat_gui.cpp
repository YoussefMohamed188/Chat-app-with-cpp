#define UNICODE
#define _UNICODE
#include <windows.h> // Core Windows GUI and API functions
#include <string>    // For std::string and std::wstring
#include <thread>    // For the background receiver thread
#include <mutex>     // For the chat log thread-safety

// --- SHARED MEMORY & SEMAPHORE CONSTANTS (UNCHANGED) ---
// Note: L prefix makes these wide-character (WCHAR) strings, necessary for UNICODE build
#define SHM_NAME L"Local\\MyChatMemory" 
#define SEM_NAME L"Local\\MyChatSemaphore"
#define SHM_SIZE 1024

// This struct defines what the shared memory looks like (UNCHANGED)
struct ChatData {
    char message[256];
    char sender[50];
    int message_id; // Acts like a version number. Increases when new message arrives.
};

// --- GLOBAL VARIABLES FOR GUI AND SHARED MEMORY (Aligned with Console App's Scope) ---
// These are made global so the receiver thread and message handler can access them.
ChatData* pBuf = nullptr;
HANDLE hMapFile = NULL;
HANDLE hSemaphore = NULL;
std::string g_username; // Global to store the user's name

// --- GUI CONSTANTS AND GLOBALS ---
#define ID_SEND_BUTTON 1001 // Unique ID for the Send button control
#define ID_INPUT_BOX 1002   // Unique ID for the text input control
#define ID_CHAT_LOG 1003    // Unique ID for the chat display area (EDIT control)

std::mutex g_chat_mutex;    // Mutex to protect the chat log (g_hChatLog) access from multiple threads
HWND g_hChatLog = NULL;     // Handle (pointer) to the chat log window control
HWND g_hInputBox = NULL;    // Handle (pointer) to the input box window control

// --- COLOR AND FONT GLOBALS ---
HBRUSH g_hBackgroundBrush = NULL; // Brush for the custom background color

// Function to append text to the chat log (Thread-safe)
void AppendToChatLog(const std::string& text) {
    // Lock guard automatically locks the mutex on creation and unlocks on destruction
    std::lock_guard<std::mutex> lock(g_chat_mutex);

    if (g_hChatLog) {
        int len = GetWindowTextLength(g_hChatLog);
        // Convert the std::string (message) to std::wstring (Windows expects wide strings)
        std::wstring wtext = std::wstring(text.begin(), text.end());

        // EM_SETSEL: Set selection to the end of the current text
        SendMessage(g_hChatLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        // EM_REPLACESEL: Replace the current selection (which is at the end) with the new text
        SendMessage(g_hChatLog, EM_REPLACESEL, 0, (LPARAM)wtext.c_str());
        // WM_VSCROLL: Scroll to the bottom to show the newest message
        SendMessage(g_hChatLog, WM_VSCROLL, SB_BOTTOM, (LPARAM)NULL);
    }
}

// --- CORE FUNCTIONALITY (Similar to receiver_thread in console app) ---
// Thread to watch for new messages
void receiver_thread(ChatData* shared_mem, std::string my_name) {
    // Safety check: wait until shared memory is fully initialized in WinMain
    while (!pBuf || !hSemaphore) {
        Sleep(100);
    }

    int last_seen_id = pBuf->message_id;

    while (true) {
        // If the ID in memory is higher than what we last saw, it's a new message!
        if (pBuf->message_id > last_seen_id) {
            std::string sender = pBuf->sender;
            std::string message = pBuf->message;
            
            // Only display if it wasn't ME who sent it
            if (sender != g_username) {
                std::string log_msg = "[" + sender + "]: " + message + "\r\n";
                // Use the thread-safe GUI function to update the chat log
                AppendToChatLog(log_msg);
            }
            last_seen_id = pBuf->message_id; // Update our tracker
        }
        Sleep(100); // Wait 0.1 seconds before checking again (saves CPU)
    }
}

// --- SEND MESSAGE LOGIC (Replaces the writing block in console app's main loop) ---
void SendMessageAction() {
    if (!pBuf || !hSemaphore) return;

    // 1. Get text from input box
    int len = GetWindowTextLength(g_hInputBox);
    if (len == 0) return;

    // Allocate memory for the wide string
    WCHAR* wText = new WCHAR[len + 1];
    // Get the text from the input control
    GetWindowText(g_hInputBox, wText, len + 1);

    // 2. Convert from WCHAR* to std::string
    std::wstring wmsg(wText);
    std::string msg(wmsg.begin(), wmsg.end());
    delete[] wText;

    // 3. Clear input box and display my message locally
    SetWindowText(g_hInputBox, L"");
    std::string log_msg = "[You]: " + msg + "\r\n";
    AppendToChatLog(log_msg);

    // 4. Write to Shared Memory (Identical logic to console app's main loop)
    // LOCK: Wait for permission to write
    WaitForSingleObject(hSemaphore, INFINITE);

    // WRITE data to RAM
    // Using strncpy to prevent buffer overflows, ensuring null termination
    strncpy(pBuf->message, msg.c_str(), sizeof(pBuf->message) - 1);
    pBuf->message[sizeof(pBuf->message) - 1] = '\0';
    strncpy(pBuf->sender, g_username.c_str(), sizeof(pBuf->sender) - 1);
    pBuf->sender[sizeof(pBuf->sender) - 1] = '\0';
    pBuf->message_id++; // Increment ID
    
    // UNLOCK: Give permission back
    ReleaseSemaphore(hSemaphore, 1, NULL);
}

// --- WINDOWS API CALLBACK ---
// The main function that Windows calls for all window messages (clicks, paints, keystrokes, etc.)
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {

        // Create a variable to store the username for the static title message
        std::wstring w_title_text = L"--- SHARED MEMORY CHAT (" + std::wstring(g_username.begin(), g_username.end()) + L") ---";

        // Create a new STATIC element and give it SS_CENTER style for centering
        HWND hTitleStatic = CreateWindowEx(0, L"STATIC", w_title_text.c_str(), 
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 5, 480, 20, // Position it slightly above the chat log
            hwnd, (HMENU)NULL, GetModuleHandle(NULL), NULL);

        // Then modify the chat log to start below it
        g_hChatLog = CreateWindowEx(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            10, 30, 480, 230, // Start from position (10, 30)
            hwnd, (HMENU)ID_CHAT_LOG, GetModuleHandle(NULL), NULL);    

        // Create the input box (single-line EDIT control)
        g_hInputBox = CreateWindowEx(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 270, 400, 30, hwnd, (HMENU)ID_INPUT_BOX, GetModuleHandle(NULL), NULL);

        // Create the Send button (BUTTON control)
        CreateWindowEx(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE,
            420, 270, 70, 30, hwnd, (HMENU)ID_SEND_BUTTON, GetModuleHandle(NULL), NULL);    
        
        // Font setup to make text readable in the controls
        HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, 
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendMessage(g_hChatLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hInputBox, WM_SETFONT, (WPARAM)hFont, TRUE);
        

        // COLOR CHANGE: Pale Mint Green background brush
        g_hBackgroundBrush = CreateSolidBrush(RGB(224, 255, 235)); 

        break;
    }

    // COLOR CHANGE: Custom coloring for controls (EDIT and STATIC)
    case WM_CTLCOLOREDIT: // Sent by EDIT controls (Chat Log, Input Box) to ask for coloring
    case WM_CTLCOLORSTATIC: // Sent by STATIC controls (Button text) to ask for coloring
    {
        HDC hdc = (HDC)wParam; // Device Context Handle
        HWND hCtrl = (HWND)lParam; // Handle of the control sending the message

        // 1. Set the Text Color (Foreground) based on control
        if (hCtrl == g_hChatLog) {
            SetTextColor(hdc, RGB(0, 100, 70)); // Dark Sea Green/Teal for chat log text
        } else if (hCtrl == g_hInputBox) {
            SetTextColor(hdc, RGB(20, 20, 20)); // Almost Black for input text
        } else {
            // Default text color for other controls (e.g., button text)
            SetTextColor(hdc, RGB(0, 128, 0)); // Medium Green for button text
        }
        
        // 2. Set the Background Mode (Must be transparent so the brush works)
        SetBkMode(hdc, TRANSPARENT);

        // 3. Return the Brush handle to paint the background
        return (LRESULT)g_hBackgroundBrush;
    }

    case WM_COMMAND: {
        // This message is sent when a control generates a notification (e.g., button click)
        int wmId = LOWORD(wParam);
        if (wmId == ID_SEND_BUTTON) {
            // Call the function to handle sending a message when the button is clicked
            SendMessageAction();
        } 
        break;
    }
    
    case WM_DESTROY:
        // This message is sent when the user closes the window.
        // Clean up the custom brush
        if (g_hBackgroundBrush) DeleteObject(g_hBackgroundBrush);

        PostQuitMessage(0); // Puts a WM_QUIT message into the queue to terminate the message loop
        break;

    default:
        // Default processing for messages we don't handle
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}


// --- MAIN ENTRY POINT (Styled to match the Console App's main function) ---
// WinMain is the standard entry point for a Windows GUI application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // --- ARGUMENT CHECK & USERNAME SETUP (Similar to argc < 2 check) ---
    // Windows GUI apps use different functions to get command-line arguments
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc); 
    if (argc < 2) {
        // If no username, generate a default one based on process ID
        g_username = "User" + std::to_string(GetCurrentProcessId());
        MessageBox(NULL, L"No username provided. Using default name.", L"Warning", MB_OK | MB_ICONWARNING); 
    } else {
        // Convert the command-line argument (wide string) to std::string
        std::wstring wname(argv[1]);
        g_username = std::string(wname.begin(), wname.end());
    }
    LocalFree(argv); // Free memory allocated by CommandLineToArgvW

    // --- 1. Create/Open Shared Memory in RAM (Identical Logic Block) ---
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    
    // --- 2. Map it so we can access it like a variable (Identical Logic Block) ---
    pBuf = (ChatData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    
    // --- 3. Create Semaphore (Lock) (Identical Logic Block) ---
    hSemaphore = CreateSemaphore(NULL, 1, 1, SEM_NAME);

    // Initialize ID if new shared memory (optional logic for first process to run)
    if (pBuf && hMapFile && GetLastError() != ERROR_ALREADY_EXISTS) {
        pBuf->message_id = 0;
    }

    if (pBuf == NULL) {
        MessageBox(NULL, L"Failed to map shared memory.", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // --- GUI SETUP (Replacing console window initialization) ---
    WNDCLASSEX wc = { 0 }; // Initialize structure for window class registration
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc; // Assign our callback function
    wc.hInstance = hInstance; // Current instance handle
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ChatAppClass"; // Unique name for the window class

    if (!RegisterClassEx(&wc)) return 0; // Register the window class

    // Create the main window instance
    HWND hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"ChatAppClass", L"Shared Memory Chat",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, // Fixed size window style
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 350, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    // --- Start background listener (Identical Logic Block) ---
    // Start the thread that continuously monitors shared memory for new messages
    std::thread t(receiver_thread, pBuf, g_username); 
    t.detach(); // Detach allows the thread to run independently

    ShowWindow(hwnd, nCmdShow); // Display the window
    UpdateWindow(hwnd);         // Force a redraw

    // --- Main Input Loop (Replaced by Windows Message Loop) ---
    // This loop is the heart of a GUI application. It retrieves messages and sends them to WndProc.
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg); // Handles virtual key codes (like ESC or ENTER)
        DispatchMessage(&msg);  // Calls WndProc for the message
    }
    // The message sending/writing is now handled inside WM_COMMAND / SendMessageAction()

    // --- Cleanup (Replaces return 0; / automatic cleanup) ---
    // Cleanly unmap and close all Windows handles before exiting
    if (pBuf) UnmapViewOfFile(pBuf);
    if (hMapFile) CloseHandle(hMapFile);
    if (hSemaphore) CloseHandle(hSemaphore);

    return msg.wParam; // Return the exit code
}