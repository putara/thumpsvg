#ifndef SVG_WINDOW_H
#define SVG_WINDOW_H


template <class T>
class Window
{
protected:
    HWND hwnd = nullptr;

    int DipToPixel(int size) const
    {
        const int dpi = this->GetDpi();
        return ::MulDiv(size, dpi, 96);
    }

    void DipToPixel(int* sizes, UINT count) const
    {
        const int dpi = this->GetDpi();
        while (count--) {
            *sizes = ::MulDiv(*sizes, dpi, 96);
            sizes++;
        }
    }

    bool SendMessageBool(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
    {
        return this->SendMessage(message, wParam, lParam) != 0;
    }

    HRESULT SendMessageHresult(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
    {
        return static_cast<HRESULT>(~this->SendMessage(message, wParam, lParam));
    }

    bool Create(ATOM atom, HINSTANCE hinst = nullptr, UINT id = 0, HWND hwndParent = nullptr, DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, DWORD exStyle = 0)
    {
        return this->Create(MAKEINTATOM(atom), hinst, id, hwndParent, style, exStyle);
    }

    bool Create(LPCTSTR className, HINSTANCE hinst = nullptr, UINT id = 0, HWND hwndParent = nullptr, DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, DWORD exStyle = 0)
    {
        HWND hwnd = ::CreateWindowEx(exStyle, className, nullptr, style, 0, 0, 0, 0, hwndParent, static_cast<HMENU>(UIntToPtr(id)), hinst, nullptr);
        this->Attach(hwnd);
        return hwnd != nullptr;
    }

public:
    void Attach(HWND hwnd)
    {
        this->hwnd = hwnd;
    }

    HWND Detach()
    {
        HWND hwnd = this->hwnd;
        this->hwnd = nullptr;
        return hwnd;
    }

    bool IsNull() const
    {
        return this->hwnd == nullptr;
    }

    HWND GetHwnd() const
    {
        return this->hwnd;
    }

    bool IsVisible() const
    {
        return ::IsWindowVisible(this->hwnd);
    }

    bool IsEnabled() const
    {
        return ::IsWindowEnabled(this->hwnd);
    }

    Rect GetRelativeWindowRect(HWND hwndParent) const
    {
        WindowRect rect(this->hwnd);
        MapWindowRect(nullptr, hwndParent, &rect);
        return rect;
    }

    int GetDpi() const
    {
        return ::GetDpiForWindow(this->hwnd);
    }

    LRESULT SendMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
    {
        return ::SendMessage(this->hwnd, message, wParam, lParam);
    }

    void ShowHide()
    {
        if (::ShowWindow(this->hwnd, SW_HIDE) == FALSE) {
            ::ShowWindow(this->hwnd, SW_SHOWNA);
        }
    }

    void Resize(const Rect& rect)
    {
        ::SetWindowPos(this->hwnd, nullptr, rect.left, rect.top,
            rect.width(), rect.height(),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void Destroy()
    {
        if (this->hwnd != nullptr) {
            ::DestroyWindow(this->hwnd);
            this->hwnd = nullptr;
        }
    }
};

#endif
