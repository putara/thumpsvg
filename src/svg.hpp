#ifndef SVG_HEADER
#define SVG_HEADER

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <resvg.h>
#pragma comment(lib, "resvg.lib")

#include "debug.hpp"
#include "svgopts.hpp"
#include "sans.hpp"

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
    default:
        // TODO: find appropriate hresult for the other errors
        return E_FAIL;
    }
}


class Svg
{
private:
    static const uint32_t MAX_SIZE = INT_MAX / 2;
    resvg_error error;
    resvg_render_tree* tree;
    resvg_size size;

    void Clear()
    {
        this->tree = nullptr;
        this->size.width = 0;
        this->size.height = 0;
    }

public:
    Svg()
    {
        this->error = RESVG_OK;
        this->Clear();
    }

    ~Svg()
    {
        this->Destroy();
    }

    Svg(Svg&& src)
    {
        this->error = src.error;
        this->tree = src.tree;
        src.tree = nullptr;
    }

    Svg(const Svg&) = delete;
    Svg& operator =(const Svg&) = delete;

    void Destroy()
    {
        if (this->tree != nullptr) {
            ::resvg_tree_destroy(this->tree);
        }
        this->Clear();
    }

    resvg_size GetSize() const
    {
        return this->size;
    }

    resvg_rect GetViewBox() const
    {
        if (this->tree == nullptr) {
            resvg_rect empty = {};
            return empty;
        }
        return ::resvg_get_image_viewbox(this->tree);
    }

    bool GetBoundingBox(resvg_rect* rect) const
    {
        ZeroMemory(rect, sizeof(*rect));
        if (this->tree == nullptr) {
            return false;
        }
        return ::resvg_get_image_bbox(this->tree, rect);
    }

    bool IsNull() const
    {
        return this->tree == nullptr;
    }

    bool IsEmpty() const
    {
        return this->tree == nullptr || !::resvg_is_image_empty(this->tree);
    }

    bool IsRenderable() const
    {
        return !this->IsEmpty() && this->size.width > 0 && this->size.height > 0;
    }

    HRESULT Load(const void* ptr, size_t cb, const SvgOptions& opt)
    {
        this->Destroy();
        Sanitiser sans;
        if (opt.GetWorkaround() && sans.Run(ptr, cb)) {
            ptr = sans.GetData();
            cb = sans.GetSize();
        }
        this->error = static_cast<resvg_error>(::resvg_parse_tree_from_data(static_cast<const char*>(ptr), cb, opt.GetOptions(), &this->tree));
        HRESULT hr = HresultFromKnownResvgError(this->error);
        if (SUCCEEDED(hr)) {
            resvg_size size = ::resvg_get_image_size(this->tree);
            if (size.width > MAX_SIZE || size.height > MAX_SIZE) {
                this->Destroy();
                return E_OUTOFMEMORY;
            }
            this->size = size;
        }
        return hr;
    }

    HRESULT Load(const wchar_t* path, const SvgOptions& opt)
    {
        this->Destroy();
        size_t cb;
        const void* ptr = mmopen(path, &cb);
        if (ptr == nullptr) {
            return HRESULT_FROM_WIN32(::GetLastError());
        }
        HRESULT hr = this->Load(ptr, cb, opt);
        mmclose(ptr);
        return hr;
    }

    template <class TRenderOptions>
    HRESULT CalcImageSize(const TRenderOptions& opt, UINT* width, UINT* height) const
    {
        if (!this->IsRenderable()) {
            *width = 0;
            *height = 0;
            return E_FAIL;
        }
        return opt.CalcImageSize(this->tree, width, height);
    }

    template <class TSvgRenderTarget>
    HRESULT Render(const typename TSvgRenderTarget::RenderOptions& opt, TSvgRenderTarget* target) const
    {
        if (!this->IsRenderable()) {
            return E_FAIL;
        }
        return TSvgRenderTarget::Render(this->tree, opt, target);
    }
};


#endif  // SVG_HEADER
