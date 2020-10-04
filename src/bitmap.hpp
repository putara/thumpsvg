#ifndef SVG_BITMAP_H
#define SVG_BITMAP_H

class Bitmap
{
protected:
    HBITMAP hbmp;

public:
    Bitmap()
    {
        this->hbmp = nullptr;
    }

    ~Bitmap()
    {
        this->Destroy();
    }

    explicit Bitmap(HBITMAP hbmp)
    {
        this->hbmp = hbmp;
    }

    Bitmap(Bitmap&& src)
    {
        this->hbmp = src.hbmp;
        src.hbmp = nullptr;
    }

    Bitmap& operator =(Bitmap&& src)
    {
        auto t = this->hbmp;
        this->hbmp = src.hbmp;
        src.hbmp = t;
        return *this;
    }

    Bitmap(const Bitmap&) = delete;
    Bitmap& operator =(const Bitmap&) = delete;

    HBITMAP GetHbitmap() const
    {
        return this->hbmp;
    }

    bool IsNull() const
    {
        return this->hbmp == nullptr;
    }

    void Destroy()
    {
        if (this->hbmp != nullptr) {
            ::DeleteObject(this->hbmp);
            this->hbmp = nullptr;
        }
    }

    void Attach(HBITMAP hbmp)
    {
        this->Destroy();
        this->hbmp = hbmp;
    }

    HBITMAP Detach()
    {
        HBITMAP hbmp = this->hbmp;
        this->hbmp = nullptr;
        return hbmp;
    }

    HBITMAP* GetAddressOf()
    {
        this->Destroy();
        return &this->hbmp;
    }

    bool GetObject(BITMAP* bm) const
    {
        return ::GetObject(this->hbmp, sizeof(*bm), bm) == sizeof(*bm);
    }

    bool GetObject(DIBSECTION* ds) const
    {
        return ::GetObject(this->hbmp, sizeof(*ds), ds) == sizeof(*ds);
    }

    SIZE GetSize() const
    {
        SIZE size = {};
        BITMAP bm = {};
        if (this->GetObject(&bm)) {
            size.cx = bm.bmWidth;
            size.cy = bm.bmHeight;
        }
        return size;
    }

    void DrawClipped(HDC hdcSrc, int x, int y, int width, int height)
    {
        if (this->hbmp == nullptr) {
            return;
        }
        HDC hdcMem = ::CreateCompatibleDC(hdcSrc);
        if (hdcMem != nullptr) {
            HGDIOBJ hbmOld = ::SelectObject(hdcMem, this->hbmp);
            if (hbmOld != nullptr) {
                ::BitBlt(hdcSrc, x, y, width, height, hdcMem, 0, 0, SRCCOPY);
                ::SelectObject(hdcMem, hbmOld);
            }
            ::DeleteDC(hdcMem);
        }
    }

    void DrawClippedAlpha(HDC hdcSrc, int x, int y, int width, int height)
    {
        if (this->hbmp == nullptr) {
            return;
        }
        HDC hdcMem = ::CreateCompatibleDC(hdcSrc);
        if (hdcMem != nullptr) {
            HGDIOBJ hbmOld = ::SelectObject(hdcMem, this->hbmp);
            if (hbmOld != nullptr) {
                const BLENDFUNCTION bf = { AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };
                ::GdiAlphaBlend(hdcSrc, x, y, width, height, hdcMem, 0, 0, width, height, bf);
                ::SelectObject(hdcMem, hbmOld);
            }
            ::DeleteDC(hdcMem);
        }
    }
};


class Bitmap32bppDIB : public Bitmap
{
public:
    Bitmap32bppDIB(int width, int height, void** bits)
    {
        *bits = nullptr;
        BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), width, height, 1, 32 } };
        this->hbmp = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, bits, nullptr, 0);
    }
};

#endif
