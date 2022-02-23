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

    float scale = 1;
    FitToType type = FitToScale;
    uint32_t width = 0;
    uint32_t height = 0;

public:
    RenderOptionsT()
    {
    }

    RenderOptionsT<T>& SetCanvasSize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || height > (UINT_MAX / 4 / width)) {
            width = 0;
            height = 0;
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


class SvgRenderTarget
{
private:
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t* pixmap = nullptr;

    HRESULT Allocate(uint32_t width, uint32_t height, char** pixmapPtr)
    {
        if (width == 0 || height == 0) {
            return E_INVALIDARG;
        }
        if (width > (UINT_MAX / 4)) {
            return E_OUTOFMEMORY;
        }
        if (height > INT_MAX || height > (UINT_MAX / 4 / width)) {
            return E_OUTOFMEMORY;
        }
        auto pixmap = static_cast<uint32_t*>(::calloc(height, width * 4));
        if (pixmap == nullptr) {
            return E_OUTOFMEMORY;
        }
        this->width = width;
        this->height = height;
        this->pixmap = pixmap;
        *pixmapPtr = reinterpret_cast<char*>(pixmap);
        return S_OK;
    }

    template <typename T>
    static void Swap(T& x, T& y)
    {
        T t;
        t = x;
        x = y;
        y = t;
    }

    template <typename T>
    static inline BYTE Clamp(T x)
    {
        return x < 0 ? 0 : (x > 255 ? 255 : static_cast<BYTE>(x));
    }

public:
    class RenderOptions : public RenderOptionsT<SvgRenderTarget::RenderOptions>
    {
    private:
        friend class SvgRenderTarget;

        void ToResvgParams(const resvg_render_tree* tree, resvg_fit_to* fitTo) const
        {
            if (this->type == FitToScale) {
                if (tree != nullptr && this->scale > 0 && this->scale != 1) {
                    fitTo->type = RESVG_FIT_TO_TYPE_ZOOM;
                    fitTo->value = this->scale;
                } else {
                    fitTo->type = RESVG_FIT_TO_TYPE_ORIGINAL;
                    fitTo->value = 1;
                }
            } else {
                if (tree == nullptr || this->width == 0 || this->height == 0) {
                    fitTo->type = RESVG_FIT_TO_TYPE_ORIGINAL;
                    fitTo->value = 1;
                } else {
                    resvg_size size = ::resvg_get_image_size(tree);
                    if (size.width == 0 || size.height == 0) {
                        fitTo->type = RESVG_FIT_TO_TYPE_ORIGINAL;
                        fitTo->value = 1;
                    } else {
                        const double scaleX = static_cast<double>(this->width) / size.width;
                        const double scaleY = static_cast<double>(this->height) / size.height;
                        if (this->type == FitToContain) {
                            if (scaleX > scaleY) {
                                fitTo->type = RESVG_FIT_TO_TYPE_HEIGHT;
                                fitTo->value = static_cast<float>(this->height);
                            } else {
                                fitTo->type = RESVG_FIT_TO_TYPE_WIDTH;
                                fitTo->value = static_cast<float>(this->width);
                            }
                        } else {
                            if (scaleX < scaleY) {
                                fitTo->type = RESVG_FIT_TO_TYPE_HEIGHT;
                                fitTo->value = static_cast<float>(this->height);
                            } else {
                                fitTo->type = RESVG_FIT_TO_TYPE_WIDTH;
                                fitTo->value = static_cast<float>(this->width);
                            }
                        }
                    }
                }
            }
        }

    public:
        HRESULT CalcImageSize(const resvg_render_tree* tree, uint32_t* width, uint32_t* height, resvg_fit_to* fitTo = nullptr) const
        {
            *width = 0;
            *height = 0;
            resvg_fit_to fitTo_ = {};
            if (fitTo == nullptr) {
                fitTo = &fitTo_;
            }
            this->ToResvgParams(tree, fitTo);
            resvg_size size = ::resvg_get_image_size(tree);
            if (size.width <= 0 || size.height <= 0) {
                return E_FAIL;
            }
            if (fitTo->type == RESVG_FIT_TO_TYPE_ORIGINAL) {
                *width = static_cast<uint32_t>(::floor(size.width));
                *height = static_cast<uint32_t>(::floor(size.height));
                return S_OK;
            } else if (fitTo->type == RESVG_FIT_TO_TYPE_ZOOM) {
                *width = static_cast<uint32_t>(::floor(size.width * fitTo->value));
                *height = static_cast<uint32_t>(::floor(size.height * fitTo->value));
                return S_OK;
            } else if (fitTo->type == RESVG_FIT_TO_TYPE_WIDTH) {
                *width = this->width;
                *height = static_cast<uint32_t>(::floor(size.height * this->width / size.width));
                return S_OK;
            } else if (fitTo->type == RESVG_FIT_TO_TYPE_HEIGHT) {
                *width = static_cast<uint32_t>(::floor(size.width * this->height / size.height));
                *height = this->height;
                return S_OK;
            }
            return E_FAIL;
        }
    };

    SvgRenderTarget()
    {
    }

    ~SvgRenderTarget()
    {
        ::free(this->pixmap);
    }

    static HRESULT Render(const resvg_render_tree* tree, const RenderOptions& opt, SvgRenderTarget* target)
    {
        uint32_t width = 0;
        uint32_t height = 0;
        char* pixmap = nullptr;
        resvg_fit_to fitTo = {};
        SvgRenderTarget self;
        HRESULT hr = opt.CalcImageSize(tree, &width, &height, &fitTo);
        if (SUCCEEDED(hr)) {
            hr = self.Allocate(width, height, &pixmap);
        }
        if (SUCCEEDED(hr)) {
            resvg_transform tx = { 1, 0, 0, 1, 0, 0 };
            ::resvg_render(tree, fitTo, tx, width, height, pixmap);
            *target = std::move(self);
        }
        return hr;
    }

    UINT GetWidth() const
    {
        return this->width;
    }

    UINT GetHeight() const
    {
        return this->height;
    }

    HRESULT ToWICBitmap(IWICImagingFactory* factory, IWICBitmapSource** pbitmap) const
    {
        *pbitmap = nullptr;
        if (this->pixmap == nullptr) {
            return E_FAIL;
        }
        uint32_t size = this->width * 4 * this->height;
        IWICBitmap* bitmap = nullptr;
        HRESULT hr = factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppPRGBA, this->width * 4, size, reinterpret_cast<BYTE*>(const_cast<uint32_t*>(this->pixmap)), &bitmap);
        if (SUCCEEDED(hr)) {
            *pbitmap = bitmap;
        }
        return hr;
    }

    HRESULT ToGdiBitmap(bool premultiplied, HBITMAP* phbmp) const
    {
        *phbmp = nullptr;
        if (this->pixmap == nullptr) {
            return E_FAIL;
        }
        int width = static_cast<int>(this->width);
        int height = static_cast<int>(this->height);
        void* bits = nullptr;
        Bitmap32bppDIB bitmap(width, height, &bits);
        if (bitmap.IsNull()) {
            return E_OUTOFMEMORY;
        }
        const uint32_t* source = this->pixmap;
        uint32_t* destination = static_cast<uint32_t*>(bits) + this->width * (this->height - 1);
        if (premultiplied) {
            for (int y = 0; y < height; y++) {
                uint32_t* dst = destination;
                for (int x = 0; x < width; x++) {
                    // TODO: SSE2
                    uint32_t src = *source++;
                    *dst++ = (src & 0xff00ff00) | ((src & 0xff) << 16) | ((src >> 16) & 0xff);
                }
                destination -= width;
            }
        } else {
            for (int y = 0; y < height; y++) {
                uint32_t* dst = destination;
                for (int x = 0; x < width; x++) {
                    // TODO: SSE2
                    uint32_t src = *source++;
                    uint8_t r = static_cast<uint8_t>(src);
                    uint8_t g = static_cast<uint8_t>(src >> 8);
                    uint8_t b = static_cast<uint8_t>(src >> 16);
                    uint8_t a = static_cast<uint8_t>(src >> 24);
                    if (a == 0) {
                        *dst++ = 0;
                    } else {
                        *dst++ = Clamp((b * 256 - 128) / a)
                               | (Clamp((g * 256 - 128) / a) << 8)
                               | (Clamp((r * 256 - 128) / a) << 16)
                               | (a << 24);
                    }
                }
                destination -= width;
            }
        }
        *phbmp = bitmap.Detach();
        return S_OK;
    }

    SvgRenderTarget(SvgRenderTarget&& src)
    {
        this->width = src.width;
        this->height = src.height;
        this->pixmap = src.pixmap;
        src.width = 0;
        src.height = 0;
        src.pixmap = nullptr;
    }

    SvgRenderTarget& operator =(SvgRenderTarget&& src)
    {
        Swap(this->width, src.width);
        Swap(this->height, src.height);
        Swap(this->pixmap, src.pixmap);
        return *this;
    }

    SvgRenderTarget(const SvgRenderTarget&) = delete;
    SvgRenderTarget& operator =(const SvgRenderTarget&) = delete;
};


#endif
