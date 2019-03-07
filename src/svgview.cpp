#define STRICT
#include <windows.h>
#include <windowsx.h>

#include "svg.hpp"


class SvgApp
{
private:
    Svg     svg;

public:
    SvgApp()
    {
    }
    ~SvgApp()
    {
    }

    void LoadSVG(HWND hwnd, const wchar_t* path)
    {
        this->svg.Load(path);
        ::InvalidateRect(hwnd, NULL, FALSE);
    }

private:
    void PaintContent(HWND hwnd, const PAINTSTRUCT& ps)
    {
        RECT rect = {};
        ::GetClientRect(hwnd, &rect);
        const int width = rect.right;
        const int height = rect.bottom;
        HBITMAP hbmp = NULL;
        if (SUCCEEDED(this->svg.RenderToBitmap(width, height, 0xffffff, false, &hbmp))) {
            HDC hdc = ::CreateCompatibleDC(ps.hdc);
            if (hdc != NULL) {
                HBITMAP hbmOld = SelectBitmap(hdc, hbmp);
                ::BitBlt(ps.hdc, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
                SelectBitmap(hdc, hbmOld);
                ::DeleteDC(hdc);
            }
            DeleteBitmap(hbmp);
        }
    }

    BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
    {
        return TRUE;
    }

    void OnDestroy(HWND hwnd)
    {
        ::PostQuitMessage(0);
    }

    void OnSize(HWND hwnd, UINT state, int cx, int cy)
    {
        ::InvalidateRect(hwnd, NULL, FALSE);
    }

    void OnDropFiles(HWND hwnd, HDROP hdrop)
    {
        wchar_t path[MAX_PATH];
        if (::DragQueryFileW(hdrop, 0, path, ARRAYSIZE(path))) {
            this->LoadSVG(hwnd, path);
        }
        ::DragFinish(hdrop);
    }

    void OnPaint(HWND hwnd)
    {
        PAINTSTRUCT ps;
        if (::BeginPaint(hwnd, &ps)) {
            this->PaintContent(hwnd, ps);
            ::EndPaint(hwnd, &ps);
        }
    }

    void OnPrintClient(HWND hwnd, HDC hdc)
    {
        PAINTSTRUCT ps = {};
        ps.hdc = hdc;
        ::GetClientRect(hwnd, &ps.rcPaint);
        this->PaintContent(hwnd, ps);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
        HANDLE_MSG(hwnd, WM_CREATE, this->OnCreate);
        HANDLE_MSG(hwnd, WM_SIZE, this->OnSize);
        HANDLE_MSG(hwnd, WM_DROPFILES, this->OnDropFiles);
        HANDLE_MSG(hwnd, WM_DESTROY, this->OnDestroy);
        HANDLE_MSG(hwnd, WM_PAINT, this->OnPaint);
        case WM_PRINTCLIENT:
            this->OnPrintClient(hwnd, (HDC)wParam);
            return 0;
        }
        return ::DefWindowProc(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        SvgApp* self = reinterpret_cast<SvgApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (self == NULL && message != WM_NCCREATE) {
            return ::DefWindowProc(hwnd, message, wParam, lParam);
        }
        if (message == WM_NCCREATE) {
            self = static_cast<SvgApp*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self->WindowProc(hwnd, message, wParam, lParam);
    }

public:
    static ATOM Register(HINSTANCE hinst)
    {
        WNDCLASS wc = {};
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpfnWndProc = StaticWindowProc;
        wc.lpszClassName = TEXT("SvgViewerWndClass");
        return ::RegisterClass(&wc);
    }

    int Run(HINSTANCE hinst, ATOM atom, int x, int y, int width, int height, const wchar_t* path)
    {
        HWND hwnd = ::CreateWindowEx(WS_EX_ACCEPTFILES, MAKEINTATOM(atom), NULL, WS_OVERLAPPEDWINDOW, x, y, width, height, NULL, NULL, hinst, this);
        if (hwnd == NULL) {
            return 2;
        }
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        if (path != NULL) {
            this->LoadSVG(hwnd, path);
        }
        MSG msg;
        msg.wParam = 3;
        while (::GetMessage(&msg, NULL, 0, 0)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        ::UnregisterClass(MAKEINTATOM(atom), hinst);
        return static_cast<int>(msg.wParam);
    }
};


int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hinstPrev, LPSTR cmdLine, int showCmd)
{
    ATOM atom;
    SvgApp app;

    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

    if ((atom = SvgApp::Register(hinst)) == 0) {
        return 1;
    }
    return app.Run(hinst, atom, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, argc > 1 ? argv[1] : NULL);
}
