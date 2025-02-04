//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

/// @file winMain.cpp
/// @brief Windows 'WinMain' entry point and event handler.
/// 
/// Implements Windows specific wrapping of frameworkApplicationBase.
//  There should be the minimum (possible) amount of code in here.

#pragma comment(linker, "/subsystem:windows")
#define NOMINMAX
#include <windows.h>
#include <string>
#include <fcntl.h>
#include <io.h>
#include <iostream>

#include "system/os_common.h"
#include "vulkan/vulkan.hpp"

#include "main/frameworkApplicationBase.hpp"
#include "main/applicationEntrypoint.hpp"
#include "gui/gui.hpp"



//=============================================================================
// GLOBALS
//=============================================================================
// Declare the application global so can access it in WndProc
FrameworkApplicationBase* gpApplication = nullptr;

#define KEY_THIS_FRAME_TIME     250

// Based on config file loading, there is a window between the application being created,
// and it is intialized.  A WM_PAINT can come in at this point and will crash because
// nothing has been initialized.
bool gAppInitialized = false;

// Flag to indicate if the gui is currently 'eating' all the mouse input events (if false the events are passed on to the application)
static bool gGuiMouseActive = false;

// If the mouse is down and being handled by the application (not the gui)
static bool gAppMouseActive = false;

// Define a funcion pointer to the GUI's winproc handler.  Use this so the GUI can catch winproc events at the lowest level (eg imgui integration)
LRESULT (*PFN_Gui_WndProcHandler)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) = nullptr/*do nothing by default*/;

int DefaultCrashReport(int i, char* msg, int* p)
{
    //print error message to stderr
    fprintf(stderr, "\n");
    fprintf(stderr, msg);

    // If debugger connected send it the message and break.
    if (IsDebuggerPresent())
    {
        OutputDebugString("\n");
        OutputDebugString(msg);
        DebugBreak();
    }
    return 0;
}
int (*PFN_CrashReportHook)(int, char*, int*) = &DefaultCrashReport;//_CRT_REPORT_HOOK)

void CreateConsoleWindow()
{
    // Create the new console window
    if (!AllocConsole())
        return;

    _CrtSetReportHook(PFN_CrashReportHook);

    // Redirect stdout and stderr from our app to the new console window...

    // Grab the output handles from windows.
    auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    auto stderrHandle = GetStdHandle(STD_ERROR_HANDLE);

    // Transfer ownership of the Windows OS handles to C style file descriptor
    FILE* pConsoleStdoutFile = _fdopen(_open_osfhandle((intptr_t)stdoutHandle, _O_TEXT), "w");
    if (pConsoleStdoutFile != nullptr)
        *stdout = *pConsoleStdoutFile;
    FILE* pConsoleStderrFile = _fdopen(_open_osfhandle((intptr_t)stderrHandle, _O_TEXT), "w");
    if (pConsoleStderrFile != nullptr)
        *stderr = *pConsoleStderrFile;

    // Disable buffering so that we dont lose output on breakpoints/crashes
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Direct C++ streams to write to stdout/stderr too
    std::ios::sync_with_stdio();
}


//-----------------------------------------------------------------------------
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
//-----------------------------------------------------------------------------
{
    uint32_t TimeNow = OS_GetTimeMS();

    if (PFN_Gui_WndProcHandler && PFN_Gui_WndProcHandler(hWnd, uMsg, wParam, lParam))
    {
        return true;
    }

    switch (uMsg)
    {
    case WM_CREATE:
        break;

    case WM_PAINT:
        if (gpApplication && gAppInitialized)
        {
            if (!gpApplication->Render())
            {
                // Exit requested.
                DestroyWindow(hWnd);
            }
        }
        break;

    case WM_SIZE:
        if (gpApplication)
            gpApplication->SetWindowSize(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_CHAR:
        break;

    case WM_KEYDOWN:
    {
        if (gpApplication && (!gpApplication->GetGui() || !gpApplication->GetGui()->WantCaptureKeyboard()))
        {
            if (HIWORD(lParam) & KF_REPEAT)
            {
                // key_down due to repeat
                gpApplication->KeyRepeatEvent((uint32_t)wParam);
            }
            else
            {
                gpApplication->KeyDownEvent((uint32_t)wParam);
            }
        }
        break;
    }

    case WM_KEYUP:
    {
        if (gpApplication && (!gpApplication->GetGui() || !gpApplication->GetGui()->WantCaptureKeyboard()))
        {
            gpApplication->KeyUpEvent((uint32_t)wParam);
        }
        break;
    }

    case WM_LBUTTONDOWN:
        if (gpApplication && (!gpApplication->GetGui() || !gpApplication->GetGui()->WantCaptureMouse()))
        {
            POINTS vMousePt = MAKEPOINTS(lParam);
            float xPos = (float)vMousePt.x;
            float yPos = (float)vMousePt.y;
            if (gpApplication->GetGui() && gpApplication->GetGui()->TouchDownEvent(0, xPos, yPos))
                gGuiMouseActive = true;

            if (!gGuiMouseActive)
            {
                //LOGI("TouchDownEvent: (%0.2f, %0.2f)", xPos, yPos);
                gpApplication->TouchDownEvent(0, xPos, yPos);
                gAppMouseActive = true;
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (gpApplication && (!gpApplication->GetGui() || !gpApplication->GetGui()->WantCaptureMouse()))
        {
            POINTS vMousePt = MAKEPOINTS(lParam);
            float xPos = (float)vMousePt.x;
            float yPos = (float)vMousePt.y;
            if (!gpApplication->GetGui() || !gpApplication->GetGui()->TouchMoveEvent(0, xPos, yPos))
            {
                if (gAppMouseActive)
                {
                    // Send the application the 'Move' event (if GUI took the 'down' event then wait until there is an 'up' event before sending anything to the application.  Garauntee Down..<Moves>..Up ordering)
                    //LOGI("TouchMoveEvent: (%0.2f, %0.2f)", xPos, yPos);
                    gpApplication->TouchMoveEvent(0, xPos, yPos);
            }
        }
        }
        break;

    case WM_LBUTTONUP:
        if (gpApplication)
        {
            POINTS vMousePt = MAKEPOINTS(lParam);
            float xPos = (float)vMousePt.x;
            float yPos = (float)vMousePt.y;
            if (!gpApplication->GetGui() || !gpApplication->GetGui()->TouchUpEvent(0, xPos, yPos))
            {
                if (gAppMouseActive)
                {
                    // Send the application the 'Touch up' event (gui is is not taking the mouse events, so the application got the 'down')
                    //LOGI("TouchUpEvent: (%0.2f, %0.2f)", xPos, yPos);
                    gpApplication->TouchUpEvent(0, xPos, yPos);
                    gAppMouseActive = false;
                }

                gGuiMouseActive = false;
            }
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

        // Default event handler
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
        break;
    }

    return 1;
}


/// This is the main entry point of the Windows app.
/// It creates a application window (which Vulkan will render to) and also a window for console output.
/// It then calls the application Application_ConstructApplication entry point,
/// initializes Vulkan and loops processing windows events (until the app is closed).
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
//-----------------------------------------------------------------------------
{
    // Grab the command line (less the program name)
    // For simplicity of not going to Unicode and using
    // this: szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    // We will just use the normal character version.
    // To get the whole command line, we could call: LPTSTR pWholeCommandLine = GetCommandLine();
    std::string sCommandLine = lpCmdLine;

    // What is this application called
    const LPCSTR pAppName = "vkSampleFramework";

    // Register the window class
    WNDCLASS wndclass = { 0 };
    wndclass.style = 0;  // CS_HREDRAW | CS_VREDRAW
    wndclass.lpfnWndProc = (WNDPROC)MainWndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, pAppName);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndclass.lpszMenuName = pAppName;
    wndclass.lpszClassName = pAppName;
    if (!RegisterClass(&wndclass))
        return FALSE;

    // Create a window for the application to render into
    // Use (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) instead of WS_CAPTION so window
    // cannot be resized.
    // DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    // WS_POPUP allows a window larger than the actual screen
    DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;

    // Create the console window
    CreateConsoleWindow();

    // Create the application
    gpApplication = Application_ConstructApplication();

    uint32_t WindowMargin = 50;

    // Want the console window here so Vulkan initialization is logged
    HWND hConsole = GetConsoleWindow();
    SetWindowPos(hConsole, 0, WindowMargin, (int)(1.0f * WindowMargin) + gSurfaceHeight, gSurfaceWidth, 3 * gSurfaceHeight / 4, SWP_SHOWWINDOW);    // SWP_NOSIZE
    freopen("CON", "w", stdout);

    // Do a very simple parse of the cmd line...
    std::string sConfigFilenameOverride;
    if (!sCommandLine.empty())
        gpApplication->SetConfigFilename(sCommandLine);

    // Load the config file
    // Need this here in order to get the window sizes
    gpApplication->LoadConfigFile();

#ifdef OS_WINDOWS
    if (gSurfaceWidth == 0)
    {
        LOGI("gSurfaceWidth => %d", gRenderWidth);
        gSurfaceWidth = gRenderWidth;
    }
    if (gSurfaceHeight == 0)
    {
        LOGI("gSurfaceHeight => %d", gRenderHeight);
        gSurfaceHeight = gRenderHeight;
    }
#endif // OS_WINDOWS

    RECT rectWindow;
    SetRect(&rectWindow, 0, 0, gSurfaceWidth, gSurfaceHeight);
    AdjustWindowRect(&rectWindow, dwStyle, FALSE);

    HWND hWnd = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
        pAppName,   // Class
        pAppName,   // Title
        dwStyle,
        WindowMargin + rectWindow.left,   // Position
        (int)(0.75f * WindowMargin + rectWindow.top),
        rectWindow.right - rectWindow.left, // Size 
        rectWindow.bottom - rectWindow.top,
        NULL,
        NULL,
        hInstance,
        NULL);
    if (!hWnd)
        return FALSE;


    // Show the window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    ::CoInitialize(NULL);

    // Initialize some Vulkan stuff
    // Do this BEFORE calling CreateVulkanWindow()
    if (!gpApplication->GetVulkan()->Init( (uintptr_t)hWnd,
                                           (uintptr_t)hInstance,
                                           [](tcb::span<const VkSurfaceFormatKHR> x) { return gpApplication->PreInitializeSelectSurfaceFormat(x); },
                                           [](Vulkan::AppConfiguration& x) { return gpApplication->PreInitializeSetVulkanConfiguration(x); }))
    {
        LOGE("Unable to initialize Vulkan!!");
        return 0;
    }

    if (!gpApplication->Initialize((uintptr_t)hWnd))
        return FALSE;

    gAppInitialized = true;

    if (!gpApplication->PostInitialize())
        return FALSE;

    // Loop on windows messages
    MSG   msg;
    bool  fDone = false;
    while (!fDone)
    {
        // Handle all the messages first
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                fDone = true;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
            {
                fDone = true;
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Application is updated in WM_PAINT

        // Show the new scene
        RedrawWindow(hWnd, NULL, NULL, RDW_INTERNALPAINT);

        // Sleep not really needed, but makes window slightly more responsive
        Sleep(1);
    }

    // Release the application
    if (gpApplication)
    {
        gpApplication->Destroy();

        delete gpApplication;
        gpApplication = NULL;
    }

    // Initialize and terminate COM for scope of this class
    ::CoInitialize(NULL);

    return 0;
}
