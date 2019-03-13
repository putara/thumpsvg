#ifndef SVG_HEADER
#define SVG_HEADER

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <cairo.h>
#include <cairo-win32.h>
#include <rsvg.h>

#include "glib.hpp"

const void* mmopen(const wchar_t* path, size_t* filesize)
{
    HANDLE hf, hfm;
    void* ptr = NULL;
    LARGE_INTEGER cb;
    cb.QuadPart = 0;
    hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf != INVALID_HANDLE_VALUE) {
        if (GetFileSizeEx(hf, &cb) && cb.HighPart == 0) {
            hfm = CreateFileMappingW(hf, NULL, PAGE_READONLY, 0, 0, NULL);
            if (hfm != NULL) {
                ptr = MapViewOfFile(hfm, FILE_MAP_READ, 0, 0, 0);
                CloseHandle(hfm);
            }
        }
        CloseHandle(hf);
    }
    *filesize = cb.LowPart;
    return ptr;
}

void mmclose(const void* ptr)
{
    UnmapViewOfFile((void*)(ptr));
}


class Svg
{
private:
    RsvgHandle*     rsvg;
    G::Error        error;
    G::Cancellable  cancellable;
    int             width;
    int             height;

    Svg(const Svg&);
    Svg& operator =(const Svg&);

public:
    Svg()
        : rsvg()
        , width()
        , height()
    {
    }

    ~Svg()
    {
        this->Close();
    }

    void Close()
    {
        if (this->rsvg != NULL) {
            ::rsvg_handle_close(this->rsvg, &this->error);
            ::g_object_unref(this->rsvg);
            this->rsvg = NULL;
        }
    }

    bool IsEmpty() const
    {
        return this->rsvg == NULL || this->width <= 0 || this->height <= 0;
    }

    void Cancel()
    {
        this->cancellable.Cancel();
    }

    HRESULT Load(const void* ptr, size_t cb)
    {
        this->Close();
        //this->rsvg = ::rsvg_handle_new_from_data(static_cast<const guint8*>(ptr), cb, &this->error);
        G::MemInputStream stream(ptr, cb);
        if (stream == false) {
            return E_OUTOFMEMORY;
        }
        this->rsvg = ::rsvg_handle_new_from_stream_sync(stream.Get(), NULL, RSVG_HANDLE_FLAGS_NONE, this->cancellable.Get(), &this->error);
        if (this->rsvg == NULL) {
            // TODO: HresultFromGError
            return E_FAIL;
        }
        RsvgDimensionData dimensions = {};
        ::rsvg_handle_get_dimensions(this->rsvg, &dimensions);
        this->width = dimensions.width;
        this->height = dimensions.height;
        return S_OK;
    }

    HRESULT Load(const wchar_t* path)
    {
        this->Close();
        size_t cb;
        const void* ptr = mmopen(path, &cb);
        if (ptr == NULL) {
            // TODO: HRESULT_FROM_WIN32
            return E_FAIL;
        }
        HRESULT hr = this->Load(ptr, cb);
        mmclose(ptr);
        return hr;
    }

    HRESULT RenderToBitmap(int width, int height, COLORREF bgcolour, bool alpha, HBITMAP* phbmp) const
    {
        *phbmp = NULL;
        if (width <= 0 || height <= 0) {
            return E_INVALIDARG;
        }
        if (this->IsEmpty()) {
            return E_FAIL;
        }
        HBITMAP hbmp = NULL;
        cairo_format_t format = alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
        cairo_surface_t* surface = ::cairo_image_surface_create(format, width, height);
        if (surface != NULL) {
            cairo_t* cr = ::cairo_create(surface);
            if (cr != NULL) {
                if (alpha == false) {
                    ::cairo_set_source_rgb(cr,
                        GetRValue(bgcolour) / 255.0,
                        GetGValue(bgcolour) / 255.0,
                        GetBValue(bgcolour) / 255.0);
                    ::cairo_rectangle(cr, 0, 0, width, height);
                    ::cairo_fill(cr);
                }
                const double x_scale = this->width <= width ? 1 : static_cast<double>(width) / this->width;
                const double y_scale = this->height <= height ? 1 : static_cast<double>(height) / this->height;
                const double scale = x_scale < y_scale ? x_scale : y_scale;
                const int new_width  = (int)floor(scale * this->width);
                const int new_height = (int)floor(scale * this->height);
                cairo_matrix_t mat = {};
                mat.yy = mat.xx = scale;
                mat.x0 = (width - new_width) / 2;
                mat.y0 = (height - new_height) / 2;
                ::cairo_transform(cr, &mat);
                ::rsvg_handle_render_cairo(this->rsvg, cr);
                ::cairo_destroy(cr);
                ::cairo_surface_flush(surface);
                BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), width, -height, 1, 32 } };
                void* bits = NULL;
                hbmp = ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
                if (hbmp != NULL) {
                    int src_stride = ::cairo_format_stride_for_width(format, width);
                    int dst_stride = width * 4;
                    const unsigned char* src = ::cairo_image_surface_get_data(surface);
                    unsigned char* dst = static_cast<unsigned char*>(bits);
                    if (src_stride == dst_stride) {
                        memcpy(dst, src, dst_stride * height);
                    } else {
                        const int copysize = dst_stride < src_stride ? dst_stride : src_stride;
                        while (height-- > 0) {
                            memcpy(dst, src, copysize);
                            dst += dst_stride;
                            src += src_stride;
                        }
                    }
                }
            }
            ::cairo_surface_destroy(surface);
        }
        *phbmp = hbmp;
        return hbmp != NULL ? S_OK : E_OUTOFMEMORY;
    }
};


#endif  // SVG_HEADER
