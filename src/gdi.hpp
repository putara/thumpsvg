#ifndef GDI_HEADER
#define GDI_HEADER


class Bitmap
{
protected:
    mutable HBITMAP hbmp;

    void Free()
    {
        if (this->hbmp != NULL) {
            ::DeleteObject(this->hbmp);
            this->hbmp = NULL;
        }
    }

private:
    Bitmap(const Bitmap&);
    Bitmap& operator =(const Bitmap&);

public:
    Bitmap()
        : hbmp()
    {
    }
    explicit Bitmap(HBITMAP hbmp)
        : hbmp(hbmp)
    {
    }
    ~Bitmap()
    {
        this->Free();
    }
    HBITMAP Detach()
    {
        HBITMAP hbmp = this->hbmp;
        this->hbmp = NULL;
        return hbmp;
    }
    HBITMAP Get() const
    {
        return this->hbmp;
    }
    HBITMAP* GetAddressOf()
    {
        this->Free();
        return &this->hbmp;
    }
    void DrawClipped(HDC hdcSrc, int x, int y, int width, int height)
    {
        if (this->hbmp == NULL) {
            return;
        }
        HDC hdcMem = ::CreateCompatibleDC(hdcSrc);
        if (hdcMem != NULL) {
            HGDIOBJ hbmOld = ::SelectObject(hdcMem, hbmp);
            if (hbmOld != NULL) {
                ::BitBlt(hdcSrc, x, y, width, height, hdcMem, 0, 0, SRCCOPY);
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
        *bits = NULL;
        BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), width, -height, 1, 32 } };
        this->hbmp = ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, bits, NULL, 0);
    }
};


#endif  // GDI_HEADER
