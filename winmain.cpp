#include <Windows.h>
#define PHYSICS

double invert_double(double value) {
    // Ensure the value is within the specified range (0 to 1)
    if (value < 0 || value > 1) {
        return value; // Or you can throw an exception here
    }

    return 1.0 - value;
}

double normalize(double value, double min, double max) {
    if (min == max) {
        return 0.5; // Handle the case where min and max are equal
    }
    return (value - min) / (max - min);
}

double clamp(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

double approximateSqrt(double value) {
    if (value <= 0) return 0;
    double guess = value / 2.0;
    for (int i = 0; i < 6; ++i) { // Perform a few iterations for a closer approximation
        guess = (guess + value / guess) / 2.0;
    }
    return guess;
}

float fastSqrt(float number) {
    long i;
    float x2, y;
    constexpr float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y = number;
    i = *(long*)&y;                      // Evil bit-level floating-point hack
    i = 0x5f3759df - (i >> 1);           // Initial magic number guess
    y = *(float*)&i;
    y = y * (threehalfs - (x2 * y * y)); // First Newton-Raphson iteration

    // Approximation for square root
    y = number * y; // Instead of taking the reciprocal, directly get the square root

    return y;
}

double calculateVelocityMagnitude(double velocityX, double velocityY) {
    return fastSqrt(
        static_cast<float>(velocityX) *
        static_cast<float>(velocityX) +
        static_cast<float>(velocityY) *
        static_cast<float>(velocityY));
}

double calculateDragCoefficient(double velocityMagnitude) {
    constexpr double maxVelocity = 1000.0; // Define an upper threshold for max velocity
    double drag = velocityMagnitude / maxVelocity; // Normalize to a 0-1 range
    return clamp(drag, 0.0, 1.0); // Ensure drag remains within [0, 1]
}

double getDrag(const double& velocityX, const double& velocityY) {
#ifdef PHYSICS
    double velocityMagnitude = calculateVelocityMagnitude(velocityX, velocityY);
    double dragCoefficient = calculateDragCoefficient(velocityMagnitude);
    return 0.1 + dragCoefficient * 0.9; // Add a base drag value for smoother dynamics
#else
    return 0.05;
#endif
}

// Define constants and macros
#define TIMER_ID_UPDATE 1
#define TIMER_INTERVAL_UPDATE 10
#define TIMER_ID_SPEED 2
#define TIMER_INTERVAL_SPEED 30

// Window Procedure function
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static double yPos = 0;
    static double xPos = 0;
    static double velocityY = 0;
    static double velocityX = 0;
    static const double gravity = 0.98;
    static const double friction = 0.90;
    double drag;
    static const double bounceFactor = 0.65;
    static bool isDragging = false;
    static bool isGroundLevel = false;
    static RECT workArea;
    static int taskbarHeight;
    static int windowHeight;
    static int windowWidth;
    static int screenHeight;
    static int screenWidth;
    static int titlebarHeight;
    static double speedX = 0.0;
    static double speedY = 0.0;
    static DWORD prevTime = 0;
    static POINT prevPos;

    switch (uMsg) {
    case WM_CREATE: {
        // Initialize variables and settings on window creation
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        int screenHeight2 = GetSystemMetrics(SM_CYSCREEN);
        taskbarHeight = screenHeight2 - (workArea.bottom - workArea.top);

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

        titlebarHeight = ncm.iCaptionHeight + GetSystemMetrics(SM_CYFRAME) * 2;

        RECT rect;
        GetWindowRect(hWnd, &rect);
        windowHeight = rect.bottom - rect.top;
        windowWidth = rect.right - rect.left;

        screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        SetTimer(hWnd, TIMER_ID_UPDATE, TIMER_INTERVAL_UPDATE, NULL);
        SetTimer(hWnd, TIMER_ID_SPEED, TIMER_INTERVAL_SPEED, NULL);
        break;
    }

    case WM_MOUSEMOVE:
        if (isDragging) {
            DWORD currentTime = GetTickCount();
            POINT currentPos;
            GetCursorPos(&currentPos);
            ScreenToClient(hWnd, &currentPos);

            if (prevTime != 0) {
                DWORD timeDiff = currentTime - prevTime;
                int deltaX = currentPos.x - prevPos.x;
                int deltaY = currentPos.y - prevPos.y;

                speedX = (double)deltaX / (timeDiff / 1000.0);
                speedY = (double)deltaY / (timeDiff / 1000.0);
            }

            prevTime = currentTime;
            prevPos = currentPos;
        }
        break;

    case WM_TIMER:
        if (wParam == TIMER_ID_SPEED) {
            DWORD currentTime2 = GetTickCount();
            POINT currentPos;
            GetCursorPos(&currentPos);

            RECT windowRect2;
            GetWindowRect(hWnd, &windowRect2);

            if (prevTime != 0) {
                DWORD timeDiff = currentTime2 - prevTime;
                int deltaX = windowRect2.left - prevPos.x;
                int deltaY = windowRect2.top - prevPos.y;

                speedX = (double)deltaX / (timeDiff / 1000.0);
                speedY = (double)deltaY / (timeDiff / 1000.0);
            }

            prevTime = currentTime2;
            prevPos.x = windowRect2.left;
            prevPos.y = windowRect2.top;
        }

        if (!isDragging && wParam == TIMER_ID_UPDATE) {
            drag = getDrag(velocityX, velocityY);

            if (velocityY > 100) velocityY = 100;
            if (velocityX > 100) velocityX = 100;
            velocityY += gravity;
            yPos += velocityY;

            // Taskbar collision
            if (yPos > screenHeight - windowHeight - titlebarHeight) {
                isGroundLevel = true;
                yPos = screenHeight - windowHeight - titlebarHeight;
                velocityY = -bounceFactor * velocityY;

                if (velocityY > 0) velocityY = -velocityY;
            }
            else {
                isGroundLevel = false;
            }

            if (isGroundLevel) velocityX *= friction * invert_double(drag * 0.05);
            else velocityX *= invert_double(drag);
            velocityY *= invert_double(drag * 0.1);
            // When in air only account for air drag
            xPos += velocityX;

            // Topside collision
            if (yPos < workArea.top) {
                yPos = workArea.top;
                velocityY *= -bounceFactor;
            }

            // Leftside collision
            if (xPos < workArea.left - 7) {
                xPos = workArea.left - 7;
                velocityX *= -bounceFactor;
            }

            // Rightside collision
            if (xPos > screenWidth - windowWidth + 7) {
                xPos = screenWidth - windowWidth + 7;
                velocityX *= -bounceFactor;
            }

            SetWindowPos(hWnd, NULL, (INT)xPos, (INT)yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        break;

    case WM_NCLBUTTONDOWN:
        if (wParam == HTCAPTION) {
            isDragging = true;
            KillTimer(hWnd, TIMER_ID_UPDATE);
        }
        DefWindowProcW(hWnd, uMsg, wParam, lParam);

    case WM_LBUTTONUP:
        if (isDragging) {
            isDragging = false;
            RECT currentPosition;
            GetWindowRect(hWnd, &currentPosition);
            yPos = currentPosition.top;
            xPos = currentPosition.left;
            velocityY = 0;
            SetTimer(hWnd, TIMER_ID_UPDATE, TIMER_INTERVAL_UPDATE, NULL);

            velocityX = speedX / 100;
            velocityY = speedY / 100;
        }
        break;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID_UPDATE);
        KillTimer(hWnd, TIMER_ID_SPEED);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR pCmdLine, int nCmdShow) {
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
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 230,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hWnd == NULL) {
        return 0;
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