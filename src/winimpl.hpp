#ifndef SVG_WINDOW_IMPL_H
#define SVG_WINDOW_IMPL_H


// WindowImpl class roughly based on:
// https://devblogs.microsoft.com/oldnewthing/20050422-08/?p=35813
template <class T>
class WindowImpl
{
private:
    HWND hwnd_ = nullptr;

protected:
    void SetHwnd(HWND hwnd)
    {
        if (this->hwnd_ == nullptr) {
            this->hwnd_ = hwnd;
        }
    }

public:
    HWND GetHwnd() const
    {
        return this->hwnd_;
    }

    void PaintContent(HWND, const PAINTSTRUCT&)
    {
    }

    BOOL OnCreate(HWND, LPCREATESTRUCT)
    {
        return TRUE;
    }

    void OnDestroy(HWND)
    {
    }

    void OnPaint(HWND hwnd)
    {
        PAINTSTRUCT ps;
        if (::BeginPaint(hwnd, &ps)) {
            T* self = static_cast<T*>(this);
            self->PaintContent(hwnd, ps);
            ::EndPaint(hwnd, &ps);
        }
    }

    void OnPrintClient(HWND hwnd, HDC hdc)
    {
        PAINTSTRUCT ps = {};
        ps.hdc = hdc;
        ::GetClientRect(hwnd, &ps.rcPaint);
        T* self = static_cast<T*>(this);
        self->PaintContent(hwnd, ps);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        T* self = static_cast<T*>(this);
        switch (message) {
        HANDLE_MSG(hwnd, WM_CREATE, self->OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, self->OnDestroy);
        HANDLE_MSG(hwnd, WM_PAINT, self->OnPaint);
        case WM_PRINTCLIENT:
            self->OnPrintClient(hwnd, (HDC)wParam);
            return 0;
        }
        return ::DefWindowProc(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        T* self = reinterpret_cast<T*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (self == nullptr && message != WM_NCCREATE) {
            return ::DefWindowProc(hwnd, message, wParam, lParam);
        }
        if (message == WM_NCCREATE) {
            self = static_cast<T*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
            self->hwnd_ = hwnd;
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        LRESULT result = self->WindowProc(hwnd, message, wParam, lParam);
        if (message == WM_NCDESTROY) {
            self->hwnd_ = nullptr;
        }
        return result;
    }

public:
    static LPCTSTR GetClassName()
    {
        return MAKEINTATOM(42);
    }

    static WNDCLASSEX GetWindowClass(HINSTANCE hinst)
    {
        WNDCLASSEX wc = { sizeof(wc) };
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpfnWndProc = T::StaticWindowProc;
        wc.lpszClassName = T::GetClassName();
        return wc;
    }

    static ATOM RegisterClass(HINSTANCE hinst)
    {
        WNDCLASSEX wc = T::GetWindowClass(hinst);
        return ::RegisterClassEx(&wc);
    }

    static T* Create(HINSTANCE hinst, ATOM atom, int x, int y, int width, int height, DWORD style, DWORD exStyle)
    {
        T* self = new T();
        if (self != nullptr) {
            HWND hwnd = ::CreateWindowEx(exStyle, MAKEINTATOM(atom), nullptr, style, x, y, width, height, nullptr, nullptr, hinst, self);
            if (hwnd == nullptr) {
                delete self;
                self = nullptr;
            }
        }
        return self;
    }
};

#endif
