#ifndef SVG_APP_H
#define SVG_APP_H

template <class T, class TWindow>
class App
{
public:
    TWindow* CreateMainWindow(HINSTANCE hinst, ATOM atom)
    {
        return TWindow::Create(hinst, atom, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, WS_OVERLAPPEDWINDOW, 0);
    }

    void OnMainWindowCreated(TWindow*)
    {
    }

    void OnMainWindowShown(TWindow*)
    {
    }

    void OnMainWindowDestroyed(TWindow*)
    {
    }

    bool OnProcessMessage(HWND, MSG&)
    {
        return false;
    }

    int Main(HINSTANCE hinst)
    {
        T* self = static_cast<T*>(this);
        ATOM atom = TWindow::RegisterClass(hinst);
        if (atom == 0) {
            return 1;
        }
        TWindow* window = self->CreateMainWindow(hinst, atom);
        if (window == nullptr) {
            return 2;
        }
        self->OnMainWindowCreated(window);
        HWND hwnd = window->GetHwnd();
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
        self->OnMainWindowShown(window);
        MSG msg;
        msg.wParam = 3;
        while (::GetMessage(&msg, NULL, 0, 0)) {
            if (self->OnProcessMessage(hwnd, msg)) {
                continue;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        self->OnMainWindowDestroyed(window);
        return static_cast<int>(msg.wParam);
    }
};

#endif
