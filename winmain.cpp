#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
#include "resource.h"

// Define constants and macros
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define TIMER_ID_MAIN 1
#define TIMER_ID_SPEED_CALC 2
#define MENU_ITEM_ID 1000

// Global Variables
static double yPos = 0;
static double xPos = 0;
static double velocityY = 0;
static double velocityX = 0;
static const double gravity = 0.90;
static const double friction = 0.90;
static const double drag = 0.99;
static const double bounceFactor = -0.75;
static bool isDragging = false;
static bool isGroundLevel = false;
static RECT workArea;
static int taskbarHeight = 40;
static int windowHeight = 200;
static int windowWidth = 500;
static int screenHeight;
static int screenWidth;
static int titlebarHeight;
static double speedX = 0.0;
static double speedY = 0.0;
static DWORD prevTime2 = 0;
static POINT prevPos;
static HDC hdcMem = NULL;
static HBITMAP hbmMem = NULL;
static HFONT hFont = NULL;
static PAINTSTRUCT ps;
static RECT rect;
static HMENU hSysMenu;

// Window Procedure function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Initialize variables and settings on window creation
        hdcMem = CreateCompatibleDC(NULL);
        GetClientRect(hwnd, &ps.rcPaint);
        hbmMem = CreateCompatibleBitmap(GetDC(hwnd), ps.rcPaint.right, ps.rcPaint.bottom);
        SelectObject(hdcMem, hbmMem);

        hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        int screenHeight2 = GetSystemMetrics(SM_CYSCREEN);
        taskbarHeight = screenHeight2 - (workArea.bottom - workArea.top);

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

        titlebarHeight = ncm.iCaptionHeight + GetSystemMetrics(SM_CYFRAME) * 2;

        GetWindowRect(hwnd, &rect);
        windowHeight = rect.bottom - rect.top;
        windowWidth = rect.right - rect.left;

        screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        SetTimer(hwnd, TIMER_ID_MAIN, 10, NULL);
        SetTimer(hwnd, TIMER_ID_SPEED_CALC, 30, NULL);

        // Add the new menu item when the window is created
        HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
        // Find the position of the "Close" item
        int closePos = GetMenuItemCount(hSysMenu) - 1; // Assuming "Close" is the last item
        // Insert the new item before "Close"
        InsertMenu(hSysMenu, closePos, MF_BYPOSITION | MF_STRING, MENU_ITEM_ID, L"Help");
        DWORD value = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        break;
    }

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
        isDragging = (hit == HTCAPTION);
        return hit;
    }

    case WM_MOUSEMOVE:
        if (isDragging) {
            DWORD currentTime = GetTickCount();
            POINT currentPos;
            GetCursorPos(&currentPos);
            ScreenToClient(hwnd, &currentPos);

            if (prevTime2 != 0) {
                DWORD timeDiff = currentTime - prevTime2;
                int deltaX = currentPos.x - prevPos.x;
                int deltaY = currentPos.y - prevPos.y;

                speedX = (double)deltaX / (timeDiff / 1000.0);
                speedY = (double)deltaY / (timeDiff / 1000.0);
            }

            prevTime2 = currentTime;
            prevPos = currentPos;
            
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID_SPEED_CALC) {
            DWORD currentTime2 = GetTickCount();
            POINT currentPos;
            GetCursorPos(&currentPos);

            RECT windowRect2;
            GetWindowRect(hwnd, &windowRect2);

            if (prevTime2 != 0) {
                DWORD timeDiff = currentTime2 - prevTime2;
                int deltaX = windowRect2.left - prevPos.x;
                int deltaY = windowRect2.top - prevPos.y;

                speedX = (double)deltaX / (timeDiff / 1000.0);
                speedY = (double)deltaY / (timeDiff / 1000.0);
            }

            prevTime2 = currentTime2;
            prevPos.x = windowRect2.left;
            prevPos.y = windowRect2.top;
        }

        if (!isDragging && wParam == TIMER_ID_MAIN) {
            if (velocityY > 100) velocityY = 100;
            if (velocityX > 100) velocityX = 100;
            velocityY += gravity;
            yPos += velocityY;

            // Taskbar collision
            if (yPos > screenHeight - windowHeight - titlebarHeight) {
                isGroundLevel = true;
                yPos = screenHeight - windowHeight - titlebarHeight;
                velocityY = bounceFactor * velocityY;

                if (velocityY > 0) velocityY = -velocityY;
            } else isGroundLevel = false;

            if (isGroundLevel) velocityX *= friction * drag;
            else velocityX *= drag;
            // When in air only account for air drag
            xPos += velocityX;

            // Topside collision
            if (yPos < workArea.top) {
                yPos = workArea.top;
                velocityY *= bounceFactor;
            }

            // Leftside collision
            if (xPos < workArea.left - 7) { // this haunts me in my dreams
                xPos = workArea.left - 7; // 7
                velocityX = bounceFactor * velocityX;
            }

            // Rightside collision
            if (xPos > screenWidth - windowWidth + 7) {
                xPos = screenWidth - windowWidth + 7;
                velocityX = bounceFactor * velocityX;
            }

            SetWindowPos(hwnd, NULL, (INT)xPos, (INT)yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_NCLBUTTONDOWN:
        if (wParam == HTCAPTION) {
            isDragging = true;
            KillTimer(hwnd, TIMER_ID_MAIN);
            DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        else {
            DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        InvalidateRect(hwnd, NULL, TRUE);

    case WM_LBUTTONUP:
        if (isDragging) {
            isDragging = false;
            RECT currentPosition;
            GetWindowRect(hwnd, &currentPosition);
            yPos = currentPosition.top;
            xPos = currentPosition.left;
            velocityY = 0;
            SetTimer(hwnd, TIMER_ID_MAIN, 10, NULL);

            velocityX = speedX / 100;
            velocityY = speedY / 100;
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_SIZE: {
        GetClientRect(hwnd, &ps.rcPaint);
        DeleteObject(hbmMem);
        hbmMem = CreateCompatibleBitmap(GetDC(hwnd), ps.rcPaint.right, ps.rcPaint.bottom);
        SelectObject(hdcMem, hbmMem);
        // memory leak my beloved

        GetWindowRect(hwnd, &rect);
        windowHeight = rect.bottom - rect.top;
        windowWidth = rect.right - rect.left;
        break;
    }

    case WM_SYSCOMMAND:
    {
        if (wParam == MENU_ITEM_ID)
        {
            KillTimer(hwnd, TIMER_ID_MAIN);
            // Handle your new menu item selection here
            MessageBoxW(hwnd, L"Bouncing window:\nYou can drop the window,\nThrow the window,\nAnd have fun!\nIf the window starts being laggy restart the app.\nhttps://github.com/maksw2/bouncy-window", L"Help", MB_OK);
            SetTimer(hwnd, TIMER_ID_MAIN, 10, NULL);
        }
        /*else if (wParam == SC_CLOSE)
        {
            xPos += 140;
        }*/
        else
        {
            // Let DefWindowProc handle other system commands
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        break;
    }

    case WM_PAINT: {
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        wchar_t buffer1[128];
        wchar_t buffer2[128];
        wchar_t buffer3[128];
        wchar_t buffer4[128];
        wchar_t buffer5[128];
        wchar_t buffer6[128];
        wchar_t buffer7[128];
        wchar_t buffer8[128];
        swprintf_s(buffer1, L"yPos: %f, velocityY: %f, speedY: %f\n", yPos, velocityY, speedY);
        swprintf_s(buffer2, L"xPos: %f, velocityX: %f, speedX: %f\n", xPos, velocityX, speedX);
        swprintf_s(buffer3, L"windowHeight: %d, windowWidth: %d, taskbarHeight: %d\n", windowHeight, windowWidth, taskbarHeight);
        swprintf_s(buffer4, L"gravity: %f, friction: %f, bounceFactor: %f, isDragging: %d\n", gravity, friction, bounceFactor, isDragging);
        swprintf_s(buffer5, L"screenHeight: %d, screenWidth: %d, isGroundLevel: %d\n", screenHeight, screenWidth, isGroundLevel);
        swprintf_s(buffer6, L"workArea.left: %d, workArea.right: %d, workArea.top: %d, workArea.bottom: %d\n", workArea.left, workArea.right, workArea.top, workArea.bottom);
        swprintf_s(buffer7, L"workArea.bottom - windowHeight: %d, rect.right: %d, rect.left: %d\n", workArea.bottom - windowHeight, rect.right, rect.left);
        swprintf_s(buffer8, L"screenWidth - windowWidth + 7: %d\n", screenWidth - windowWidth + 7);

        FillRect(hdcMem, &clientRect, (HBRUSH)(COLOR_WINDOW + 3));
        SelectObject(hdcMem, hFont);
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(255, 255, 255));

        TextOut(hdcMem, 10, 10, buffer1, (INT)wcslen(buffer1));
        TextOut(hdcMem, 10, 30, buffer2, (INT)wcslen(buffer2));
        TextOut(hdcMem, 10, 50, buffer3, (INT)wcslen(buffer3));
        TextOut(hdcMem, 10, 70, buffer4, (INT)wcslen(buffer4));
        TextOut(hdcMem, 10, 90, buffer5, (INT)wcslen(buffer5));
        TextOut(hdcMem, 10, 110, buffer6, (INT)wcslen(buffer6));
        TextOut(hdcMem, 10, 130, buffer7, (INT)wcslen(buffer7));
        TextOut(hdcMem, 10, 150, buffer8, (INT)wcslen(buffer8));

        BitBlt(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom, hdcMem, 0, 0, SRCCOPY);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID_MAIN);
        KillTimer(hwnd, TIMER_ID_SPEED_CALC);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        DeleteObject(hFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Main function
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR pCmdLine, int nCmdShow) {
    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BouncingWindowClass";

    RegisterClass(&wc);

    // Create window
    HWND hwnd = CreateWindowExW(
        0,                                    // Opcjonalne style okna
        L"BouncingWindowClass",             // Nazwa klasy okna
        L"Bouncy window",                     // Tytuł okna
        WS_OVERLAPPEDWINDOW,                  // Styl okna
        CW_USEDEFAULT, CW_USEDEFAULT, 680, 230, // Rozmiar i pozycja
        NULL,                                 // Uchwyt okna nadrzędnego
        NULL,                                 // Uchwyt menu
        hInstance,                            // Uchwyt instancji aplikacji
        NULL                                  // Dodatkowe dane okna
    );

    if (hwnd == NULL) {
        return 0;
    }

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    if (hIcon)
    {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    ShowWindow(hwnd, nCmdShow);

    // Start messages
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}