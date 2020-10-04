#ifndef SVG_RENDER_H
#define SVG_RENDER_H

#include <wincodec.h>
#include "bitmap.hpp"


template <class T>
class RenderOptionsT
{
protected:
    enum FitToType
    {
        FitToScale,
        FitToContain,
        FitToCover,
    };

    friend class Svg;

    static const COLORREF Transparent = 0xFFFFFFFFL;

    float scale = 1;
    FitToType type = FitToScale;
    COLORREF bgColour = Transparent;
    UINT width = 0;
    UINT height = 0;

public:
    RenderOptionsT()
    {
    }

    RenderOptionsT<T>& SetBgColour(COLORREF colour)
    {
        this->bgColour = colour & 0xFFFFFF;
        return *this;
    }

    RenderOptionsT<T>& SetTransparent()
    {
        this->bgColour = Transparent;
        return *this;
    }

    RenderOptionsT<T>& SetCanvasSize(UINT width, UINT height)
    {
        if (width > INT_MAX || height > INT_MAX) {
            return *this;
        }
        this->width = width;
        this->height = height;
        return *this;
    }

    RenderOptionsT<T>& SetScale(float scale)
    {
        this->type = FitToScale;
        this->scale = scale;
        return *this;
    }

    RenderOptionsT<T>& SetToContain()
    {
        this->type = FitToContain;
        return *this;
    }

    RenderOptionsT<T>& SetToCover()
    {
        this->type = FitToCover;
        return *this;
    }
};

#ifdef SVG_RT_CAIRO
class SvgRenderTargetCairo
{
private:
    cairo_surface_t* surface = nullptr;

    SvgRenderTargetCairo(cairo_surface_t* surface)
    {
        this->surface = surface;
    }

public:
    class RenderOptions : public RenderOptionsT<SvgRenderTargetCairo::RenderOptions>
    {
    private:
        friend class SvgRenderTargetCairo;

        float CalcScaling(const resvg_render_tree* tree) const
        {
            float scale;
            if (this->type == FitToScale) {
                if (tree != nullptr && this->scale > 0 && this->scale != 1) {
                    scale = this->scale;
                } else {
                    scale = 1;
                }
            } else {
                if (tree == nullptr || this->width == 0 || this->height == 0) {
                    scale = 1;
                } else {
                    resvg_size size = ::resvg_get_image_size(tree);
                    if (size.width == 0 || size.height == 0) {
                        scale = 1;
                    } else {
                        const float scaleX = static_cast<float>(this->width) / size.width;
                        const float scaleY = static_cast<float>(this->height) / size.height;
                        if (this->type == FitToContain) {
                            if (scaleX > scaleY) {
                                scale = scaleY;
                            } else {
                                scale = scaleX;
                            }
                        } else {
                            if (scaleX < scaleY) {
                                scale = scaleY;
                            } else {
                                scale = scaleX;
                            }
                        }
                    }
                }
            }
            return scale;
        }

    public:
        HRESULT CalcImageSize(const resvg_render_tree* tree, UINT* width, UINT* height) const
        {
            float scale = this->CalcScaling(tree);
            resvg_size size = ::resvg_get_image_size(tree);
            *width  = static_cast<UINT>(scale * size.width);
            *height = static_cast<UINT>(scale * size.height);
            return S_OK;
        }
    };

    SvgRenderTargetCairo()
    {
        this->surface = nullptr;
    }

    ~SvgRenderTargetCairo()
    {
        if (this->surface != nullptr) {
            ::cairo_surface_destroy(this->surface);
        }
    }

    static HRESULT Render(const resvg_render_tree* tree, const RenderOptions& opt, SvgRenderTargetCairo* target)
    {
        UINT width, height;
        opt.CalcImageSize(tree, &width, &height);
        cairo_surface_t* surface = ::cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cairo_t* cr = nullptr;
        if (surface != nullptr) {
            cr = ::cairo_create(surface);
            if (cr != nullptr) {
                if (opt.bgColour != RenderOptions::Transparent) {
                    ::cairo_set_source_rgb(cr,
                        GetRValue(opt.bgColour) / 255.0,
                        GetGValue(opt.bgColour) / 255.0,
                        GetBValue(opt.bgColour) / 255.0);
                    ::cairo_rectangle(cr, 0, 0, width, height);
                    ::cairo_fill(cr);
                }
                resvg_size size;
                size.width = static_cast<uint32_t>(width);
                size.height = static_cast<uint32_t>(height);
                ::resvg_cairo_render_to_canvas(tree, size, cr);
                ::cairo_destroy(cr);
                ::cairo_surface_flush(surface);
            } else {
                ::cairo_surface_destroy(surface);
                surface = nullptr;
            }
        }

        *target = std::move(SvgRenderTargetCairo(surface));
        return surface != nullptr ? S_OK : E_FAIL;
    }

    UINT GetWidth() const
    {
        if (this->surface != nullptr) {
            return ::cairo_image_surface_get_width(this->surface);
        }
        return 0;
    }

    UINT GetHeight() const
    {
        if (this->surface != nullptr) {
            return ::cairo_image_surface_get_width(this->surface);
        }
        return 0;
    }

    HRESULT ToWICBitmap(IWICImagingFactory* factory, IWICBitmapSource** pbitmap) const
    {
        *pbitmap = nullptr;
        if (this->surface == nullptr) {
            return E_FAIL;
        }
        const uint32_t width = this->GetWidth();
        const uint32_t height = this->GetHeight();
        if (width > INT_MAX || height > INT_MAX) {
            return E_OUTOFMEMORY;
        }
        const int stride = ::cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
        if (stride <= 0) {
            return E_FAIL;
        }
        if (static_cast<uint64_t>(stride) * height > UINT_MAX) {
            return E_OUTOFMEMORY;
        }
        const unsigned char* data = ::cairo_image_surface_get_data(this->surface);
        if (data == nullptr) {
            return E_OUTOFMEMORY;
        }
        const UINT size = static_cast<UINT>(stride) * height;
        IWICBitmap* bitmap = nullptr;
        HRESULT hr = factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppPBGRA, stride, size, const_cast<BYTE*>(data), &bitmap);
        if (SUCCEEDED(hr)) {
            *pbitmap = bitmap;
        }
        return hr;
    }

    HRESULT ToGdiBitmap(bool premultiplied, HBITMAP* phbmp) const
    {
        *phbmp = nullptr;
        if (this->surface == nullptr) {
            return E_FAIL;
        }
        const uint32_t width = this->GetWidth();
        const uint32_t height = this->GetHeight();
        if (width > INT_MAX || height > INT_MAX) {
            return E_OUTOFMEMORY;
        }
        const int stride = ::cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
        if (stride <= 0) {
            return E_FAIL;
        }
        const uint8_t* source = ::cairo_image_surface_get_data(this->surface);
        if (source == nullptr) {
            return E_OUTOFMEMORY;
        }
        void* bits = nullptr;
        Bitmap32bppDIB bitmap(static_cast<int>(width), static_cast<int>(height), &bits);
        if (bitmap.IsNull()) {
            return E_OUTOFMEMORY;
        }
        size_t remains = static_cast<UINT>(stride) * height;
        const UINT padding = static_cast<UINT>(stride) - width * 4;
        uint32_t* destination = static_cast<uint32_t*>(bits) + width * (height - 1);
        if (premultiplied) {
            for (uint32_t y = 0; y < height && remains; y++) {
                uint32_t* dst = destination;
                for (uint32_t x = 0; x < width && remains; x++, remains--) {
                    // TODO: SSE2
                    *dst++ = *reinterpret_cast<const uint32_t*>(source);
                    source += 4;
                }
                source += padding;
                destination -= width;
            }
        } else {
            for (uint32_t y = 0; y < height && remains; y++) {
                uint32_t* dst = destination;
                for (uint32_t x = 0; x < width && remains; x++, remains--) {
                    // TODO: SSE2
                    uint8_t b = *source++;
                    uint8_t g = *source++;
                    uint8_t r = *source++;
                    uint8_t a = *source++;
                    if (a > 0) {
                        *dst++ = ((b * 255 + (a / 2)) / a)
                               | (((g * 255 + (a / 2)) / a) << 8)
                               | (((r * 255 + (a / 2)) / a) << 16)
                               | (a << 24);
                    } else {
                        *dst++ = 0;
                    }
                }
                source += padding;
                destination -= width;
            }
        }
        *phbmp = bitmap.Detach();
        return S_OK;
    }

    SvgRenderTargetCairo(SvgRenderTargetCairo&& src)
    {
        this->surface = src.surface;
        src.surface = nullptr;
    }

    SvgRenderTargetCairo& operator =(SvgRenderTargetCairo&& src)
    {
        auto t = this->surface;
        this->surface = src.surface;
        src.surface = t;
        return *this;
    }

    SvgRenderTargetCairo(const SvgRenderTargetCairo&) = delete;
    SvgRenderTargetCairo& operator =(const SvgRenderTargetCairo&) = delete;
};

typedef SvgRenderTargetCairo SvgRenderTarget;
#endif


#ifdef SVG_RT_SKIA
class SvgRenderTargetSkia
{
private:
    resvg_image* image = nullptr;

    SvgRenderTargetSkia(resvg_image* image)
    {
        this->image = image;
    }

public:
    class RenderOptions : public RenderOptionsT<SvgRenderTargetSkia::RenderOptions>
    {
    private:
        friend class SvgRenderTargetSkia;

        resvg_color* ToResvgParams(const resvg_render_tree* tree, resvg_fit_to* fitTo, resvg_color* colour) const
        {
            if (this->type == FitToScale) {
                if (tree != nullptr && this->scale > 0 && this->scale != 1) {
                    fitTo->type = RESVG_FIT_TO_ZOOM;
                    fitTo->value = this->scale;
                } else {
                    fitTo->type = RESVG_FIT_TO_ORIGINAL;
                    fitTo->value = 1;
                }
            } else {
                if (tree == nullptr || this->width == 0 || this->height == 0) {
                    fitTo->type = RESVG_FIT_TO_ORIGINAL;
                    fitTo->value = 1;
                } else {
                    resvg_size size = ::resvg_get_image_size(tree);
                    if (size.width == 0 || size.height == 0) {
                        fitTo->type = RESVG_FIT_TO_ORIGINAL;
                        fitTo->value = 1;
                    } else {
                        const float scaleX = static_cast<float>(this->width) / size.width;
                        const float scaleY = static_cast<float>(this->height) / size.height;
                        if (this->type == FitToContain) {
                            if (scaleX > scaleY) {
                                fitTo->type = RESVG_FIT_TO_HEIGHT;
                                fitTo->value = static_cast<float>(this->height);
                            } else {
                                fitTo->type = RESVG_FIT_TO_WIDTH;
                                fitTo->value = static_cast<float>(this->width);
                            }
                        } else {
                            if (scaleX < scaleY) {
                                fitTo->type = RESVG_FIT_TO_HEIGHT;
                                fitTo->value = static_cast<float>(this->height);
                            } else {
                                fitTo->type = RESVG_FIT_TO_WIDTH;
                                fitTo->value = static_cast<float>(this->width);
                            }
                        }
                    }
                }
            }
            if (colour == nullptr) {
                return nullptr;
            }
            if (this->bgColour == Transparent) {
                ZeroMemory(colour, sizeof(*colour));
                return nullptr;
            }
            colour->r = GetRValue(this->bgColour);
            colour->g = GetGValue(this->bgColour);
            colour->b = GetBValue(this->bgColour);
            return colour;
        }

    public:
        HRESULT CalcImageSize(const resvg_render_tree* tree, UINT* width, UINT* height) const
        {
            *width = 0;
            *height = 0;
            resvg_fit_to fitTo = {};
            this->ToResvgParams(tree, &fitTo, nullptr);
            resvg_size size = ::resvg_get_image_size(tree);
            if (fitTo.type == RESVG_FIT_TO_ORIGINAL) {
                *width = size.width;
                *height = size.height;
                return S_OK;
            } else if (fitTo.type == RESVG_FIT_TO_ZOOM) {
                *width = static_cast<UINT>(size.width * fitTo.value);
                *height = static_cast<UINT>(size.height * fitTo.value);
                return S_OK;
            } else if (fitTo.type == RESVG_FIT_TO_WIDTH) {
                *width = static_cast<UINT>(fitTo.value);
                *height = static_cast<UINT>(size.height * (fitTo.value / size.width));
                return S_OK;
            } else if (fitTo.type == RESVG_FIT_TO_HEIGHT) {
                *width = static_cast<UINT>(size.width * (fitTo.value / size.height));
                *height = static_cast<UINT>(fitTo.value);
                return S_OK;
            }
            return E_FAIL;
        }
    };

    SvgRenderTargetSkia()
    {
        this->image = nullptr;
    }

    ~SvgRenderTargetSkia()
    {
        if (this->image != nullptr) {
            ::resvg_image_destroy(this->image);
        }
    }

    static HRESULT Render(const resvg_render_tree* tree, const RenderOptions& opt, SvgRenderTargetSkia* target)
    {
        resvg_fit_to fitTo = {};
        resvg_color colour_ = {};
        resvg_color* colour = opt.ToResvgParams(tree, &fitTo, &colour_);
        resvg_image* image = ::resvg_render(tree, fitTo, colour);
        *target = std::move(SvgRenderTargetSkia(image));
        return image != nullptr ? S_OK : E_FAIL;
    }

    UINT GetWidth() const
    {
        if (this->image != nullptr) {
            return ::resvg_image_get_width(this->image);
        }
        return 0;
    }

    UINT GetHeight() const
    {
        if (this->image != nullptr) {
            return ::resvg_image_get_height(this->image);
        }
        return 0;
    }

    HRESULT ToWICBitmap(IWICImagingFactory* factory, IWICBitmapSource** pbitmap) const
    {
        *pbitmap = nullptr;
        if (this->image == nullptr) {
            return E_FAIL;
        }
        uint32_t width = ::resvg_image_get_width(this->image);
        uint32_t height = ::resvg_image_get_height(this->image);
        if (width > INT_MAX || height > INT_MAX) {
            return E_OUTOFMEMORY;
        }
        size_t size = 0;
        const BYTE* data = reinterpret_cast<const BYTE*>(::resvg_image_get_data(this->image, &size));
        if (size > UINT_MAX) {
            return E_OUTOFMEMORY;
        }
        IWICBitmap* bitmap = nullptr;
        HRESULT hr = factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppRGBA, width * 4, static_cast<UINT>(size), const_cast<BYTE*>(data), &bitmap);
        if (SUCCEEDED(hr)) {
            *pbitmap = bitmap;
        }
        return hr;
    }

    HRESULT ToGdiBitmap(bool premultiplied, HBITMAP* phbmp) const
    {
        *phbmp = nullptr;
        if (this->image == nullptr) {
            return E_FAIL;
        }
        uint32_t width = ::resvg_image_get_width(this->image);
        uint32_t height = ::resvg_image_get_height(this->image);
        if (width > INT_MAX || height > INT_MAX) {
            return E_OUTOFMEMORY;
        }
        void* bits = nullptr;
        Bitmap32bppDIB bitmap(static_cast<int>(width), static_cast<int>(height), &bits);
        if (bitmap.IsNull()) {
            return E_OUTOFMEMORY;
        }
        size_t remains = 0;
        const uint32_t* source = reinterpret_cast<const uint32_t*>(::resvg_image_get_data(this->image, &remains));
        uint32_t* destination = static_cast<uint32_t*>(bits) + width * (height - 1);
        if (premultiplied) {
            for (uint32_t y = 0; y < height && remains; y++) {
                uint32_t* dst = destination;
                for (uint32_t x = 0; x < width && remains; x++, remains--) {
                    // TODO: SSE2
                    uint32_t src = *source++;
                    uint8_t r = static_cast<uint8_t>(src);
                    uint8_t g = static_cast<uint8_t>(src >> 8);
                    uint8_t b = static_cast<uint8_t>(src >> 16);
                    uint8_t a = static_cast<uint8_t>(src >> 24);
                    *dst++ = ((b * a + 128) >> 8)
                           | (((g * a + 128) >> 8) << 8)
                           | (((r * a + 128) >> 8) << 16)
                           | (a << 24);
                }
                destination -= width;
            }
        } else {
            for (uint32_t y = 0; y < height && remains; y++) {
                uint32_t* dst = destination;
                for (uint32_t x = 0; x < width && remains; x++, remains--) {
                    // TODO: SSE2
                    uint32_t src = *source++;
                    *dst++ = (src & 0xff00ff00) | ((src & 0xff) << 16) | ((src >> 16) & 0xff);
                }
                destination -= width;
            }
        }
        *phbmp = bitmap.Detach();
        return S_OK;
    }

    SvgRenderTargetSkia(SvgRenderTargetSkia&& src)
    {
        this->image = src.image;
        src.image = nullptr;
    }

    SvgRenderTargetSkia& operator =(SvgRenderTargetSkia&& src)
    {
        auto t = this->image;
        this->image = src.image;
        src.image = t;
        return *this;
    }

    SvgRenderTargetSkia(const SvgRenderTargetSkia&) = delete;
    SvgRenderTargetSkia& operator =(const SvgRenderTargetSkia&) = delete;
};

typedef SvgRenderTargetSkia SvgRenderTarget;
#endif


#endif
