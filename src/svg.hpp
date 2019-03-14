#ifndef SVG_HEADER
#define SVG_HEADER

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <cairo.h>
#include <cairo-win32.h>

extern "C" {
#define RESVG_CAIRO_BACKEND
#include <resvg.h>
}

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

HRESULT HresultFromKnownResvgError(resvg_error error)
{
    switch (error) {
    case RESVG_OK:
        return S_OK;
    case RESVG_ERROR_NO_CANVAS:
        return E_OUTOFMEMORY;
    default:
        // TODO: find appropriate hresult for the other errors
        return E_FAIL;
    }
}

class Svg
{
private:
    int                 width;
    int                 height;
    resvg_error         error;
    resvg_render_tree*  tree;

    Svg(const Svg&);
    Svg& operator =(const Svg&);

public:
    Svg()
        : width()
        , height()
        , error(RESVG_OK)
        , tree()
    {
    }

    ~Svg()
    {
        this->Close();
    }

    void Close()
    {
        if (this->tree != NULL) {
            ::resvg_tree_destroy(this->tree);
            this->tree = NULL;
        }
        this->height = this->width = 0;
    }

    bool IsEmpty() const
    {
        return this->tree == NULL || this->width <= 0 || this->height <= 0;
    }

    HRESULT Load(const void* ptr, size_t cb)
    {
        this->Close();
        resvg_options opt = this->GetOption();
        this->error = static_cast<resvg_error>(::resvg_parse_tree_from_data(static_cast<const char*>(ptr), cb, &opt, &this->tree));
        HRESULT hr = HresultFromKnownResvgError(this->error);
        if (SUCCEEDED(hr)) {
            resvg_size size = ::resvg_get_image_size(this->tree);
            if (size.width > INT_MAX || size.height > INT_MAX) {
                this->Close();
                return E_OUTOFMEMORY;
            }
            this->width = static_cast<int>(size.width);
            this->height = static_cast<int>(size.height);
        }
        return hr;
    }

    HRESULT Load(const wchar_t* path)
    {
        this->Close();
        size_t cb;
        const void* ptr = mmopen(path, &cb);
        if (ptr == NULL) {
            return HRESULT_FROM_WIN32(::GetLastError());
        }
        HRESULT hr = this->Load(ptr, cb);
        mmclose(ptr);
        return hr;
    }

    resvg_options GetOption() const
    {
        resvg_options opt = {};
        ::resvg_init_options(&opt);
        // TODO: use EnumFontFamilies()
        opt.font_family = "Microsoft Sans Serif";
        // TODO: use GetSystemPreferredUILanguages()
        opt.languages = "en";
        return opt;
    }

    HRESULT RenderToBitmap(int width, int height, COLORREF bgcolour, bool alpha, HBITMAP* phbmp) const
    {
        *phbmp = NULL;
        if (width <= 0 || height <= 0) {
            return E_INVALIDARG;
        }

        cairo_format_t format = alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
        cairo_surface_t* surface = ::cairo_image_surface_create(format, width, height);
        if (surface == NULL) {
            return E_OUTOFMEMORY;
        }

        cairo_t* cr = ::cairo_create(surface);
        if (cr == NULL) {
            ::cairo_surface_destroy(surface);
            return E_OUTOFMEMORY;
        }

        if (alpha == false) {
            ::cairo_set_source_rgb(cr,
                GetRValue(bgcolour) / 255.0,
                GetGValue(bgcolour) / 255.0,
                GetBValue(bgcolour) / 255.0);
            ::cairo_rectangle(cr, 0, 0, width, height);
            ::cairo_fill(cr);
        }

        const bool empty = this->IsEmpty();
        if (empty == false) {
            const double x_scale = this->width <= width ? 1 : static_cast<double>(width) / this->width;
            const double y_scale = this->height <= height ? 1 : static_cast<double>(height) / this->height;
            const double scale = x_scale < y_scale ? x_scale : y_scale;
            const int new_width  = (int)floor(scale * this->width);
            const int new_height = (int)floor(scale * this->height);

            resvg_options opt = this->GetOption();
            opt.fit_to.type = RESVG_FIT_TO_ZOOM;
            opt.fit_to.value = scale;

            resvg_size size;
            size.width = static_cast<uint32_t>(width);
            size.height = static_cast<uint32_t>(height);
            ::resvg_cairo_render_to_canvas(this->tree, &opt, size, cr);
            ::cairo_destroy(cr);
            ::cairo_surface_flush(surface);
        }

        const BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), width, -height, 1, 32 } };
        void* bits = NULL;
        HBITMAP hbmp =::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (hbmp == NULL) {
            ::cairo_surface_destroy(surface);
            return E_OUTOFMEMORY;
        }

        const int src_stride = ::cairo_format_stride_for_width(format, width);
        const int dst_stride = width * 4;
        const unsigned char* src = ::cairo_image_surface_get_data(surface);
        unsigned char* dst = static_cast<unsigned char*>(bits);
        if (src_stride == dst_stride) {
            ::memcpy(dst, src, dst_stride * height);
        } else {
            const int copysize = dst_stride < src_stride ? dst_stride : src_stride;
            while (height-- > 0) {
                ::memcpy(dst, src, copysize);
                dst += dst_stride;
                src += src_stride;
            }
        }

        *phbmp = hbmp;
        ::cairo_surface_destroy(surface);
        return empty ? S_FALSE : S_OK;
    }
};


#endif  // SVG_HEADER
