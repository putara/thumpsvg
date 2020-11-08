#ifndef SVG_VIEWER_WINDOW_H
#define SVG_VIEWER_WINDOW_H


#include "winimpl.hpp"
#include "viewer.hpp"
#include "bitmap.hpp"
#include "brush.hpp"

class SvgViewerImpl : public WindowImpl<SvgViewerImpl>, public NoThrowObject
{
private:
    Svg svg;
    Bitmap cache;
    Brush checker;
    bool showViewBox = false;
    bool showBBox = false;
    int zoom = SVGZOOM_CONTAIN;

    SvgViewerImpl(HWND hwnd) noexcept
    {
        this->SetHwnd(hwnd);
    }

    ~SvgViewerImpl() noexcept
    {
    }

    void Invalidate(HWND hwnd, bool always = false)
    {
        ::InvalidateRect(hwnd, nullptr, FALSE);

        auto size = this->svg.GetSize();
        if (size.width == 0 || size.height == 0) {
            this->cache.Destroy();
            return;
        }

        SvgRenderTarget::RenderOptions opt;
        ClientRect rect(hwnd);
        opt.SetTransparent().SetCanvasSize(rect.width(), rect.height());

        if (this->zoom == SVGZOOM_CONTAIN) {
            opt.SetToContain();
        } else if (this->zoom == SVGZOOM_COVER) {
            opt.SetToCover();
        } else {
            opt.SetScale(static_cast<float>(1 / 100.) * this->zoom);
        }

        if (always) {
            this->cache.Destroy();
        } else if (!this->cache.IsNull()) {
            SIZE size = this->cache.GetSize();
            UINT width, height;
            this->svg.CalcImageSize(opt, &width, &height);
            if (size.cx == width && size.cy == height) {
                // not necessary
                return;
            }
        }
        if (rect.right <= 0 || rect.bottom <= 0) {
            return;
        }
        SvgRenderTarget target;
        // TODO: background thread
        if (SUCCEEDED(this->svg.Render(opt, &target))) {
            Bitmap bmp;
            if (SUCCEEDED(target.ToGdiBitmap(true, bmp.GetAddressOf()))) {
                this->cache = std::move(bmp);
            }
        }
    }

    void CreateCheckerBoard(HWND hwnd)
    {
        int dpi = ::GetDpiForWindow(hwnd);
        int cellSize = ::MulDiv(8, dpi, 96);
        Bitmap bmp(::CreateBitmap(cellSize * 2, cellSize * 2, 1, 1, nullptr));
        if (!bmp.IsNull()) {
            HDC hdc = ::CreateCompatibleDC(nullptr);
            if (hdc != nullptr) {
                HBITMAP oldBmp = SelectBitmap(hdc, bmp.GetHbitmap());
                ::PatBlt(hdc, 0, 0, cellSize * 2, cellSize * 2, WHITENESS);
                ::PatBlt(hdc, 0, 0, cellSize, cellSize, BLACKNESS);
                ::PatBlt(hdc, cellSize, cellSize, cellSize, cellSize, BLACKNESS);
                SelectBitmap(hdc, oldBmp);
                ::DeleteDC(hdc);
                Brush brush(::CreatePatternBrush(bmp.GetHbitmap()));
                this->checker = std::move(brush);
            }
        }
    }

    HRESULT UpdateBoolValue(HWND hwnd, LPARAM value, bool* param)
    {
        bool oldValue = *param;
        bool newValue = value != 0;
        if (oldValue != newValue) {
            *param = newValue;
            if (this->svg.IsNull() == false) {
                this->Invalidate(hwnd);
            }
        }
        return oldValue ? S_OK : S_FALSE;
    }

    HRESULT UpdateIntValue(HWND hwnd, LPARAM value, int* param)
    {
        int oldValue = *param;
        int newValue = static_cast<int>(value);
        if (oldValue != newValue) {
            *param = newValue;
            if (this->svg.IsNull() == false) {
                this->Invalidate(hwnd);
            }
        }
        return oldValue ? S_OK : S_FALSE;
    }

    static LRESULT LR(bool x)
    {
        return x;
    }

    static LRESULT LR(HRESULT x)
    {
        return ~x;
    }

public:
    static LPCTSTR GetClassName()
    {
        return TEXT("SvgViewer");
    }

    void PaintContent(HWND hwnd, const PAINTSTRUCT& ps)
    {
        ClientRect rect(hwnd);
        if (::IsRectEmpty(&rect)) {
            return;
        }

        HDC hdc = ps.hdc;
        if (!this->checker.IsNull()) {
            COLORREF oldBg = ::SetBkColor(hdc, 0xdddddd);
            COLORREF oldFg = ::SetTextColor(hdc, 0xaaaaaa);
            ::FillRect(hdc, &rect, this->checker.GetHbrush());
            ::SetBkColor(hdc, oldBg);
            ::SetTextColor(hdc, oldFg);
        } else {
            ::FillRect(hdc, &rect, static_cast<HBRUSH>(IntToPtr(COLOR_WINDOW + 1)));
        }

        if (!this->cache.IsNull()) {
            SIZE size = this->cache.GetSize();
            int x = rect.left + (rect.width() - size.cx) / 2;
            int y = rect.top + (rect.height() - size.cy) / 2;
            this->cache.DrawClippedAlpha(hdc, x, y, size.cx, size.cy);
            if (this->showViewBox || this->showBBox) {
                resvg_rect rrect = this->svg.GetViewBox();
                if (rrect.width && rrect.height) {
                    double scaleX = static_cast<double>(size.cx) / rrect.width;
                    double scaleY = static_cast<double>(size.cy) / rrect.height;
                    if (this->showViewBox) {
                        DrawRectangle(hdc, RGB(255, 0, 0),
                            x + static_cast<int>(::floor(rrect.x * scaleX)),
                            y + static_cast<int>(::floor(rrect.y * scaleY)),
                            static_cast<int>(::ceil(rrect.width * scaleX)),
                            static_cast<int>(::ceil(rrect.height * scaleY)));
                    }
                    if (this->showBBox) {
                        resvg_rect rrect = {};
                        if (this->svg.GetBoundingBox(&rrect)) {
                            DrawRectangle(hdc, RGB(0, 0, 255),
                                x + static_cast<int>(::floor(rrect.x * scaleX)),
                                y + static_cast<int>(::floor(rrect.y * scaleY)),
                                static_cast<int>(::ceil(rrect.width * scaleX)) + 1,
                                static_cast<int>(::ceil(rrect.height * scaleY)) + 1);
                        }
                    }
                }
            }
        }
    }

    static void DrawRectangle(HDC hdc, COLORREF color, int x, int y, int width, int height)
    {
        HPEN oldPen = SelectPen(hdc, GetStockPen(DC_PEN));
        HBRUSH oldBrush = SelectBrush(hdc, GetStockBrush(HOLLOW_BRUSH));
        COLORREF oldColor = ::SetDCPenColor(hdc, color);
        ::Rectangle(hdc, x, y, x + width, y + height);
        ::SetDCPenColor(hdc, oldColor);
        SelectBrush(hdc, oldBrush);
        SelectPen(hdc, oldPen);
    }

    BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
    {
        this->CreateCheckerBoard(hwnd);
        return __super::OnCreate(hwnd, lpcs);
    }

    void OnSize(HWND hwnd, UINT state, int cx, int cy)
    {
        this->Invalidate(hwnd);
    }

    void OnCopy(HWND hwnd)
    {
        SvgRenderTarget::RenderOptions opt;
        SvgRenderTarget target;
        Bitmap bmp;
        DIBSECTION ds = {};
        HGLOBAL hglob = nullptr;

        HRESULT hr = this->svg.Render(opt, &target);
        if (SUCCEEDED(hr)) {
            hr = target.ToGdiBitmap(false, bmp.GetAddressOf());
        }
        if (SUCCEEDED(hr)) {
            hr = bmp.GetObject(&ds) ? S_OK : ResultFromLastError();
        }
        if (SUCCEEDED(hr)) {
            hr = ds.dsBm.bmPlanes == 1 && ds.dsBm.bmBitsPixel == 32 ? S_OK : E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            hr = UInt32x32To64(ds.dsBm.bmWidthBytes, ds.dsBm.bmHeight) <= (INT_MAX - sizeof(BITMAPV5HEADER)) ? S_OK : E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hr)) {
            const size_t cb = sizeof(BITMAPV5HEADER) + ds.dsBm.bmWidthBytes * ds.dsBm.bmHeight;
            hglob = ::GlobalAlloc(GHND | GMEM_SHARE, cb);
            hr = hglob != nullptr ? S_OK : E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hr)) {
            BITMAPV5HEADER* bv5h = reinterpret_cast<BITMAPV5HEADER*>(::GlobalLock(hglob));
            int dpi = ::GetDpiForWindow(hwnd);
            bv5h->bV5Size = sizeof(*bv5h);
            bv5h->bV5Width = ds.dsBmih.biWidth;
            bv5h->bV5Height = ds.dsBmih.biHeight;
            bv5h->bV5Planes = 1;
            bv5h->bV5BitCount = 32;
            bv5h->bV5XPelsPerMeter = static_cast<LONG>(dpi * 39.375);
            bv5h->bV5YPelsPerMeter = static_cast<LONG>(dpi * 39.375);
            bv5h->bV5RedMask = 0x00FF0000;
            bv5h->bV5GreenMask = 0x0000FF00;
            bv5h->bV5BlueMask = 0x000000FF;
            bv5h->bV5AlphaMask = 0xFF000000;
            bv5h->bV5CSType = LCS_sRGB;
            bv5h->bV5Intent = LCS_GM_IMAGES;
            ::memcpy(bv5h + 1, ds.dsBm.bmBits, ds.dsBm.bmWidthBytes * ds.dsBm.bmHeight);
            ::GlobalUnlock(hglob);
        }
        if (hglob != nullptr) {
            HANDLE handle = nullptr;
            if (::OpenClipboard(hwnd)) {
                ::EmptyClipboard();
                handle = ::SetClipboardData(CF_DIBV5, hglob);
                ::CloseClipboard();
            }
            if (handle == nullptr) {
                ::GlobalFree(hglob);
            }
        }
    }

    void OnDpiChanged(HWND hwnd, int dpiX, int dpiY, const RECT* newScaleRect)
    {
        this->CreateCheckerBoard(hwnd);
    }

    bool OnSvgIsLoaded(HWND) const
    {
        return this->svg.IsNull() == false;
    }

    bool OnSvgIsEmpty(HWND) const
    {
        return this->svg.IsEmpty();
    }

    HRESULT OnSvgGetSize(HWND hwnd, SvgSizeU* size) const
    {
        if (size == nullptr) {
            return E_POINTER;
        }
        resvg_size rsize = this->svg.GetSize();
        size->width = rsize.width;
        size->height = rsize.height;
        return this->svg.IsNull() ? S_FALSE : S_OK;
    }

    HRESULT OnSvgGetViewBox(HWND hwnd, SvgRectF* rect) const
    {
        if (rect == nullptr) {
            return E_POINTER;
        }
        resvg_rect rrect = this->svg.GetViewBox();
        rect->x = rrect.x;
        rect->y = rrect.y;
        rect->width = rrect.width;
        rect->height = rrect.height;
        return this->svg.IsNull() ? S_FALSE : S_OK;
    }

    HRESULT OnSvgGetBoundingBox(HWND hwnd, SvgRectF* rect) const
    {
        if (rect == nullptr) {
            return E_POINTER;
        }
        resvg_rect rrect = {};
        if (this->svg.GetBoundingBox(&rrect)) {
            rect->x = rrect.x;
            rect->y = rrect.y;
            rect->width = rrect.width;
            rect->height = rrect.height;
            return S_OK;
        }
        SvgRectF empty = {};
        *rect = empty;
        return S_FALSE;
    }

    HRESULT OnSvgDpiChanged(HWND hwnd)
    {
        this->CreateCheckerBoard(hwnd);
        this->Invalidate(hwnd);
        return S_OK;
    }

    HRESULT OnSvgRefresh(HWND hwnd)
    {
        this->CreateCheckerBoard(hwnd);
        this->Invalidate(hwnd, true);
        return S_OK;
    }

    HRESULT OnSvgLoad(HWND hwnd, UINT_PTR type, const void* param)
    {
        if (type != 0 || param == nullptr) {
            return E_INVALIDARG;
        }
        return this->OnSvgLoadFromFile(hwnd, static_cast<LPCWSTR>(param));
    }

    HRESULT OnSvgLoadFromFile(HWND hwnd, LPCWSTR path)
    {
        SvgOptions opt;
        opt.LoadSystemFonts();
        HRESULT hr = this->svg.Load(path, opt);
        this->Invalidate(hwnd, true);
        return hr;
    }

    HRESULT OnSvgClose(HWND hwnd)
    {
        this->svg.Destroy();
        this->cache.Destroy();
        ::InvalidateRect(hwnd, nullptr, FALSE);
        return S_OK;
    }

    HRESULT OnSvgExport(HWND hwnd, UINT type, LPCWSTR path)
    {
        if (path == nullptr) {
            return E_INVALIDARG;
        }
        if (type == SVGEXP_PNG) {
            return this->OnSvgSaveToPNG(path);
        }
        return E_INVALIDARG;
    }

    HRESULT OnSvgSaveToPNG(const wchar_t* path)
    {
        IWICImagingFactory* factory = nullptr;
        IWICBitmapSource* source = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        SvgRenderTarget::RenderOptions opt;
        SvgRenderTarget target;

        HRESULT hr = ::WICCreateImagingFactory_Proxy(WINCODEC_SDK_VERSION, &factory);
        if (SUCCEEDED(hr)) {
            hr = this->svg.Render(opt, &target);
        }
        if (SUCCEEDED(hr)) {
            hr = target.ToWICBitmap(factory, &source);
        }
        if (SUCCEEDED(hr)) {
            hr = factory->CreateStream(&stream);
        }
        if (SUCCEEDED(hr)) {
            hr = stream->InitializeFromFilename(path, GENERIC_WRITE);
        }
        if (SUCCEEDED(hr)) {
            GUID guid = GUID_VendorMicrosoftBuiltIn;
            hr = factory->CreateEncoder(GUID_ContainerFormatPng, &guid, &encoder);
        }
        if (SUCCEEDED(hr)) {
            hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
        }
        if (SUCCEEDED(hr)) {
            hr = encoder->CreateNewFrame(&frame, nullptr);
        }
        if (SUCCEEDED(hr)) {
            hr = frame->Initialize(nullptr);
        }
        if (SUCCEEDED(hr)) {
            WICPixelFormatGUID guid = GUID_WICPixelFormat32bppRGBA;
            hr = frame->SetPixelFormat(&guid);
        }
        if (SUCCEEDED(hr)) {
            const UINT width = target.GetWidth();
            const UINT height = target.GetHeight();
            hr = frame->SetSize(width, height);
            if (SUCCEEDED(hr)) {
                WICRect rect;
                rect.X = 0;
                rect.Y = 0;
                rect.Width = width;
                rect.Height = height;
                hr = frame->WriteSource(source, &rect);
            }
        }
        if (SUCCEEDED(hr)) {
            hr = frame->Commit();
        }
        if (SUCCEEDED(hr)) {
            hr = encoder->Commit();
        }
        if (frame != nullptr) {
            frame->Release();
        }
        if (encoder != nullptr) {
            encoder->Release();
        }
        if (stream != nullptr) {
            stream->Release();
        }
        if (source != nullptr) {
            source->Release();
        }
        if (factory != nullptr) {
            factory->Release();
        }
        return hr;
    }

    HRESULT OnSvgGetOption(HWND, UINT option, LPARAM lParam)
    {
        int* uparam;
        switch (option) {
        case SVGOPT_ZOOM:
            uparam = reinterpret_cast<int*>(lParam);
            if (uparam == nullptr) {
                return E_POINTER;
            }
            *uparam = this->zoom;
            return S_OK;
        case SVGOPT_VIEWBOX:
            return this->showViewBox ? S_OK : S_FALSE;
        case SVGOPT_BBOX:
            return this->showBBox ? S_OK : S_FALSE;
        }
        UNREFERENCED_PARAMETER(lParam);
        return E_INVALIDARG;
    }

    HRESULT OnSvgSetOption(HWND hwnd, UINT option, LPARAM lParam)
    {
        switch (option) {
        case SVGOPT_ZOOM:
            if (lParam > 0 || lParam == SVGZOOM_CONTAIN || lParam == SVGZOOM_COVER) {
                return this->UpdateIntValue(hwnd, lParam, &this->zoom);
            }
            break;
        case SVGOPT_VIEWBOX:
            return this->UpdateBoolValue(hwnd, lParam, &this->showViewBox);
        case SVGOPT_BBOX:
            return this->UpdateBoolValue(hwnd, lParam, &this->showBBox);
        }
        return E_INVALIDARG;
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
        HANDLE_MSG(hwnd, WM_SIZE, this->OnSize);
        HANDLE_MSG(hwnd, WM_COPY, this->OnCopy);
        case WM_DPICHANGED:
            return this->OnDpiChanged(hwnd, HIWORD(wParam), LOWORD(wParam), reinterpret_cast<const RECT*>(lParam)), 0L;
        case SVGWM_IS_LOADED:
            return LR(this->OnSvgIsLoaded(hwnd));
        case SVGWM_IS_EMPTY:
            return LR(this->OnSvgIsEmpty(hwnd));
        case SVGWM_GET_SIZE:
            return LR(this->OnSvgGetSize(hwnd, reinterpret_cast<SvgSizeU*>(lParam)));
        case SVGWM_GET_VIEW_BOX:
            return LR(this->OnSvgGetViewBox(hwnd, reinterpret_cast<SvgRectF*>(lParam)));
        case SVGWM_GET_BOUNDING_BOX:
            return LR(this->OnSvgGetBoundingBox(hwnd, reinterpret_cast<SvgRectF*>(lParam)));
        case SVGWM_GET_OPTION:
            return LR(this->OnSvgGetOption(hwnd, static_cast<UINT>(wParam), lParam));
        case SVGWM_SET_OPTION:
            return LR(this->OnSvgSetOption(hwnd, static_cast<UINT>(wParam), lParam));
        case SVGWM_LOAD:
            return LR(this->OnSvgLoad(hwnd, wParam, reinterpret_cast<void*>(lParam)));
        case SVGWM_CLOSE:
            return LR(this->OnSvgClose(hwnd));
        case SVGWM_EXPORT:
            return LR(this->OnSvgExport(hwnd, static_cast<uint8_t>(wParam), reinterpret_cast<LPCWSTR>(lParam)));
        case SVGWM_DPI_CHANGED:
            return LR(this->OnSvgDpiChanged(hwnd));
        case SVGWM_REFRESH:
            return LR(this->OnSvgRefresh(hwnd));
        }
        return __super::WindowProc(hwnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        SvgViewerImpl* self = reinterpret_cast<SvgViewerImpl*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (self == nullptr && message != WM_NCCREATE) {
            return ::DefWindowProc(hwnd, message, wParam, lParam);
        }
        if (message == WM_NCCREATE) {
            self = new SvgViewerImpl(hwnd);
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        LRESULT result = self->WindowProc(hwnd, message, wParam, lParam);
        if (message == WM_NCDESTROY) {
            delete self;
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return result;
    }
};


#endif
