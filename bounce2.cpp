#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
#include <cmath>
#include "resource.h"

// Define constants and macros
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define BOOL(x) x ? L"true" : L"false"
#define TIMER_ID_MAIN 1
#define TIMER_INTERVAL_MAIN 10
#define TIMER_ID_SPEED 2
#define TIMER_INTERVAL_SPEED 30
#define MENU_ITEM_ID_HELP 1000
#define MENU_ITEM_ID_THEME 1001
#define MENU_ITEM_ID_PAUSE 1002

// Window Procedure function
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Position and motion
    static double xPos = 0;
    static double yPos = 0;
    static double velocityX = 0;
    static double velocityY = 0;
    static double speedX = 0.0;
    static double speedY = 0.0;

    // Physics constants
    static const double gravity = 0.90;
    static const double friction = 0.90;
    static const double drag = 0.99;
    static const double bounceFactor = -0.75;

    // State flags
    static bool isDragging = false;
    static bool isGroundLevel = false;
    static bool isPaused = false;
    static bool darkMode;

    // Screen and window dimensions
    static int screenWidth;
    static int screenHeight;
    static int windowWidth;
    static int windowHeight;
    static int taskbarHeight;
    static int titlebarHeight;

    // Window-related structs
    static RECT workArea;
    static RECT rect;
    static POINT prevPos;
    static PAINTSTRUCT ps;
    static ULONGLONG prevTime = 0;

    // Graphics
    static HDC hdcMem = NULL;
    static HBITMAP hbmMem = NULL;
    static HFONT hFont = NULL;
    static HMENU hSysMenu;

    switch (uMsg) {
    case WM_CREATE: {
        // Initialize variables and settings on window creation
        hdcMem = CreateCompatibleDC(NULL);
        GetClientRect(hWnd, &ps.rcPaint);
        hbmMem = CreateCompatibleBitmap(GetDC(hWnd), ps.rcPaint.right, ps.rcPaint.bottom);
        SelectObject(hdcMem, hbmMem);

        hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        int screenHeight2 = GetSystemMetrics(SM_CYSCREEN);
        taskbarHeight = screenHeight2 - (workArea.bottom - workArea.top);

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

        titlebarHeight = ncm.iCaptionHeight + GetSystemMetrics(SM_CYFRAME) * 2;

        GetWindowRect(hWnd, &rect);
        windowHeight = rect.bottom - rect.top;
        windowWidth = rect.right - rect.left;

        screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        SetTimer(hWnd, TIMER_ID_MAIN, TIMER_INTERVAL_MAIN, NULL);
        SetTimer(hWnd, TIMER_ID_SPEED, TIMER_INTERVAL_SPEED, NULL);

        // Add the new menu item when the window is created
        HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
        // Find the position of the "Close" item
        int closePos = GetMenuItemCount(hSysMenu) - 1; // Assuming "Close" is the last item
        // Insert the new item before "Close"
        InsertMenu(hSysMenu, closePos, MF_BYPOSITION | MF_STRING, MENU_ITEM_ID_HELP, L"Help");
        InsertMenu(hSysMenu, closePos + 1, MF_BYPOSITION | MF_STRING, MENU_ITEM_ID_THEME, L"Dark theme");
        InsertMenu(hSysMenu, closePos + 2, MF_BYPOSITION | MF_STRING, MENU_ITEM_ID_PAUSE, L"Pause simulation");
        break;
    }

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hWnd, uMsg, wParam, lParam);
        isDragging = (hit == HTCAPTION);
        return hit;
    }

    case WM_MOUSEMOVE:
        if (isDragging) {
            ULONGLONG currentTime = GetTickCount64();  // safer
            POINT currentPos;
            GetCursorPos(&currentPos);
            ScreenToClient(hWnd, &currentPos);

            if (prevTime != 0) {
                ULONGLONG timeDiff = currentTime - prevTime;
                int deltaX = currentPos.x - prevPos.x;
                int deltaY = currentPos.y - prevPos.y;

                speedX = (double)deltaX / (timeDiff / 1000.0);
                speedY = (double)deltaY / (timeDiff / 1000.0);
            }

            prevTime = currentTime;
            prevPos = currentPos;

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID_SPEED) {
            ULONGLONG currentTime2 = GetTickCount64();  // use 64-bit time
            POINT currentPos;
            GetCursorPos(&currentPos);

            RECT windowRect2;
            GetWindowRect(hWnd, &windowRect2);

            if (prevTime != 0) {
                ULONGLONG timeDiff = currentTime2 - prevTime;
                int deltaX = windowRect2.left - prevPos.x;
                int deltaY = windowRect2.top - prevPos.y;

                speedX = (double)deltaX / (timeDiff / 1000.0);
                speedY = (double)deltaY / (timeDiff / 1000.0);
            }

            prevTime = currentTime2;
            prevPos.x = windowRect2.left;
            prevPos.y = windowRect2.top;
        }

        if ((!isDragging && !isPaused) && wParam == TIMER_ID_MAIN) {
            velocityY += gravity;
            yPos += velocityY;

            // Taskbar collision
            if (yPos > screenHeight - windowHeight - titlebarHeight) {
                isGroundLevel = true;
                yPos = screenHeight - windowHeight - titlebarHeight;
                velocityY = bounceFactor * velocityY;

                if (velocityY > 0) velocityY = -velocityY;
            }
            else isGroundLevel = false;

            if (isGroundLevel) velocityX *= friction * drag;
            else velocityX *= drag; // When in air only account for air drag

            xPos += velocityX;

            // Topside collision
            if (yPos < workArea.top) {
                yPos = workArea.top;
                velocityY *= bounceFactor;
            }

            // Leftside collision
            if (xPos < workArea.left - 7) { // this haunts me in my dreams
                xPos = workArea.left - 7;
                velocityX = bounceFactor * velocityX;
            }

            // Rightside collision
            if (xPos > screenWidth - windowWidth + 7) {
                xPos = screenWidth - windowWidth + 7;
                velocityX = bounceFactor * velocityX;
            }

            SetWindowPos(hWnd, NULL, static_cast<INT>(std::round(xPos)), static_cast<INT>(std::round(yPos)), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_NCLBUTTONDOWN:
        if (wParam == HTCAPTION) {
            isDragging = true;
            KillTimer(hWnd, TIMER_ID_MAIN);
            DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        else {
            DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        InvalidateRect(hWnd, NULL, TRUE);
        [[fallthrough]];

    case WM_LBUTTONUP:
        if (isDragging) {
            isDragging = false;
            RECT currentPosition;
            GetWindowRect(hWnd, &currentPosition);
            yPos = currentPosition.top;
            xPos = currentPosition.left;
            velocityY = 0;
            SetTimer(hWnd, TIMER_ID_MAIN, TIMER_INTERVAL_MAIN, NULL);

            velocityX = speedX / 100;
            velocityY = speedY / 100;
        }
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_SIZE: {
        RECT newClientRect;
        GetClientRect(hWnd, &newClientRect);
        int newWidth = newClientRect.right - newClientRect.left;
        int newHeight = newClientRect.bottom - newClientRect.top;

        // Check if the size has actually changed
        if (newWidth > 0 && newHeight > 0 && (newWidth != ps.rcPaint.right || newHeight != ps.rcPaint.bottom)) {
            // Delete the old bitmap
            DeleteObject(hbmMem);

            // Create a new bitmap with the new dimensions
            HDC hdc = GetDC(hWnd);
            hbmMem = CreateCompatibleBitmap(hdc, newWidth, newHeight);
            ReleaseDC(hWnd, hdc);

            // Select the new bitmap into the memory DC
            SelectObject(hdcMem, hbmMem);

            // Update the PAINTSTRUCT's rect
            ps.rcPaint = newClientRect;
        }

        // Update window dimensions
        GetWindowRect(hWnd, &rect);

        windowHeight = rect.bottom - rect.top;
        windowWidth = rect.right - rect.left;

        xPos = rect.left;
        yPos = rect.top;

        DefWindowProc(hWnd, uMsg, wParam, lParam);

        break;
    }

    case WM_SYSCOMMAND:
    {
        if (wParam == MENU_ITEM_ID_HELP)
        {
            KillTimer(hWnd, TIMER_ID_MAIN);
            MessageBoxW(hWnd, L"uhh, yeah\nhttps://github.com/maksw2/bouncy-window", L"Help", MB_OK);
            SetTimer(hWnd, TIMER_ID_MAIN, TIMER_INTERVAL_MAIN, NULL);
        }
        else if (wParam == MENU_ITEM_ID_THEME)
        {
            KillTimer(hWnd, TIMER_ID_MAIN);
            darkMode = !darkMode;
            if (darkMode)
            {
                DWORD value = TRUE;
                DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
                ShowWindow(hWnd, SW_HIDE);
                ShowWindow(hWnd, SW_SHOW);
            }
            else
            {
                DWORD value = FALSE;
                DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
                ShowWindow(hWnd, SW_HIDE);
                ShowWindow(hWnd, SW_SHOW);
            }
        }
        else if (wParam == MENU_ITEM_ID_PAUSE)
        {
            isPaused = !isPaused;
        }
        else
        {
            // Let DefWindowProc handle other system commands
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        break;
    }

    case WM_PAINT: {
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        wchar_t buffer1[128];
        wchar_t buffer2[128];
        wchar_t buffer3[128];
        wchar_t buffer4[128];
        wchar_t buffer5[128];
        wchar_t buffer6[128];
        wchar_t buffer7[128];
        wchar_t buffer8[128];

        swprintf_s(buffer1, L"xPos: %.1f, yPos: %.1f  |  velocityX: %.2f, velocityY: %.2f  |  speedX: %.2f, speedY: %.2f",
            xPos, yPos, velocityX, velocityY, speedX, speedY);

        swprintf_s(buffer2, L"windowWidth: %d, windowHeight: %d  |  screenWidth: %d, screenHeight: %d",
            windowWidth, windowHeight, screenWidth, screenHeight);

        swprintf_s(buffer3, L"taskbarHeight: %d  |  titlebarHeight: %d",
            taskbarHeight, titlebarHeight);

        swprintf_s(buffer4, L"workArea L:%d R:%d T:%d B:%d  |  rect.left: %d, rect.right: %d",
            workArea.left, workArea.right, workArea.top, workArea.bottom, rect.left, rect.right);

        swprintf_s(buffer5, L"workArea.bottom - windowHeight: %d  |  screenWidth - windowWidth + 7: %d",
            workArea.bottom - windowHeight, screenWidth - windowWidth + 7);

        swprintf_s(buffer6, L"gravity: %.2f  |  friction: %.2f  |  drag: %.2f  |  bounceFactor: %.2f",
            gravity, friction, drag, bounceFactor);

        swprintf_s(buffer7, L"isDragging: %s  |  isGroundLevel: %s  |  isPaused: %s  |  darkMode: %s",
            BOOL(isDragging),
            isGroundLevel ? L"true" : L"false",
            isPaused ? L"true" : L"false",
            darkMode ? L"true" : L"false");

        swprintf_s(buffer8, L"prevTime: %llu  |  prevPos.x: %ld, prevPos.y: %ld",
            prevTime, prevPos.x, prevPos.y);

        if (darkMode) FillRect(hdcMem, &clientRect, (HBRUSH)(COLOR_WINDOW + 3)); else FillRect(hdcMem, &clientRect, (HBRUSH)(COLOR_WINDOW + 1));
        SelectObject(hdcMem, hFont);
        SetBkMode(hdcMem, TRANSPARENT);
        if (darkMode) SetTextColor(hdcMem, RGB(255, 255, 255)); else SetTextColor(hdcMem, RGB(0, 0, 0));

        TextOut(hdcMem, 10, 10, buffer1, (INT)wcslen(buffer1));
        TextOut(hdcMem, 10, 30, buffer2, (INT)wcslen(buffer2));
        TextOut(hdcMem, 10, 50, buffer3, (INT)wcslen(buffer3));
        TextOut(hdcMem, 10, 70, buffer4, (INT)wcslen(buffer4));
        TextOut(hdcMem, 10, 90, buffer5, (INT)wcslen(buffer5));
        TextOut(hdcMem, 10, 110, buffer6, (INT)wcslen(buffer6));
        TextOut(hdcMem, 10, 130, buffer7, (INT)wcslen(buffer7));
        TextOut(hdcMem, 10, 150, buffer8, (INT)wcslen(buffer8));

        BitBlt(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom, hdcMem, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID_MAIN);
        KillTimer(hWnd, TIMER_ID_SPEED);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        DeleteObject(hFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow) {
    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BouncingWindowClass";

    RegisterClass(&wc);

    // Create window
    HWND hWnd = CreateWindowExW(
        0,
        L"BouncingWindowClass",
        L"Bouncy window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 230,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hWnd == NULL) {
        return 0;
    }

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BOUNCE2));
    if (hIcon)
    {
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    ShowWindow(hWnd, nCmdShow);

    // Start messages
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}