#ifndef SVG_RECT_H
#define SVG_RECT_H

class Rect : public RECT
{
public:
    Rect()
    {
        // random filled
    }

    Rect(int left, int top, int right, int bottom)
    {
        this->left = left;
        this->top = top;
        this->right = right;
        this->bottom = bottom;
    }

    Rect(const RECT& source)
    {
        operator =(source);
    }

    Rect& operator =(const RECT& source)
    {
        *static_cast<RECT*>(this) = source;
        return *this;
    }

    int width() const
    {
        return this->right - this->left;
    }

    int height() const
    {
        return this->bottom - this->top;
    }

    POINT CenterPoint() const
    {
        POINT pt = {
            this->left + this->width() / 2,
            this->top + this->height() / 2,
        };
        return pt;
    }

    void Empty()
    {
        ZeroMemory(static_cast<RECT*>(this), sizeof(RECT));
    }
};

class WindowRect : public Rect
{
public:
    WindowRect(HWND hwnd)
    {
        this->Empty();
        ::GetWindowRect(hwnd, this);
    }
};

class ClientRect : public Rect
{
public:
    ClientRect(HWND hwnd)
    {
        this->Empty();
        ::GetClientRect(hwnd, this);
    }
};

#endif
