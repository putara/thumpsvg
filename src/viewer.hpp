#ifndef SVG_VIEWER_API_H
#define SVG_VIEWER_API_H


#include "thumpsvg.h"

constexpr UINT SVGWM_FIRST             = WM_USER + 0x200;

constexpr UINT SVGWM_IS_LOADED         = SVGWM_FIRST + 0;
constexpr UINT SVGWM_IS_EMPTY          = SVGWM_FIRST + 1;
constexpr UINT SVGWM_GET_SIZE          = SVGWM_FIRST + 2;
constexpr UINT SVGWM_GET_VIEW_BOX      = SVGWM_FIRST + 3;
constexpr UINT SVGWM_GET_BOUNDING_BOX  = SVGWM_FIRST + 4;
constexpr UINT SVGWM_GET_OPTION        = SVGWM_FIRST + 5;
constexpr UINT SVGWM_SET_OPTION        = SVGWM_FIRST + 6;
constexpr UINT SVGWM_LOAD              = SVGWM_FIRST + 7;
constexpr UINT SVGWM_CLOSE             = SVGWM_FIRST + 8;
constexpr UINT SVGWM_EXPORT            = SVGWM_FIRST + 9;
constexpr UINT SVGWM_DPI_CHANGED       = SVGWM_FIRST + 10;
constexpr UINT SVGWM_REFRESH           = SVGWM_FIRST + 11;

// SVGWM_GET_OPTION/SVGWM_SET_OPTION
constexpr UINT SVGOPT_ZOOM             = 1;
constexpr UINT SVGOPT_VIEWBOX          = 2;
constexpr UINT SVGOPT_BBOX             = 3;
constexpr UINT SVGOPT_SPEED            = 4;

// SVGOPT_ZOOM
constexpr int SVGZOOM_CONTAIN          = -1;
constexpr int SVGZOOM_COVER            = -2;

// SVGWM_EXPORT
constexpr UINT SVGEXP_PNG              = 0;
constexpr UINT SVGEXP_BMP              = 1;
constexpr UINT SVGEXP_GIF              = 2;


struct SvgSizeF
{
    DOUBLE width;
    DOUBLE height;
};

struct SvgRectF {
    DOUBLE x;
    DOUBLE y;
    DOUBLE width;
    DOUBLE height;
};

#include "rect.hpp"
#include "window.hpp"


class SvgViewer : public Window<SvgViewer>
{
public:
    bool IsLoaded() const
    {
        return this->SendMessageBool(SVGWM_IS_LOADED);
    }

    bool IsEmpty() const
    {
        return this->SendMessageBool(SVGWM_IS_EMPTY);
    }

    SvgSizeF GetSize() const
    {
        SvgSizeF size = {};
        if (SUCCEEDED(this->SendMessageHresult(SVGWM_GET_SIZE, 0, reinterpret_cast<LPARAM>(&size)))) {
            return size;
        }
        SvgSizeF empty = {};
        return empty;
    }

    SvgRectF GetViewBox() const
    {
        SvgRectF rect = {};
        if (SUCCEEDED(this->SendMessageHresult(SVGWM_GET_VIEW_BOX, 0, reinterpret_cast<LPARAM>(&rect)))) {
            return rect;
        }
        SvgRectF empty = {};
        return empty;
    }

    HRESULT GetBoundingBox(SvgRectF* rect) const
    {
        return this->SendMessageHresult(SVGWM_GET_BOUNDING_BOX, 0, reinterpret_cast<LPARAM>(rect));
    }

    HRESULT LoadFromFile(LPCWSTR path)
    {
        return this->SendMessageHresult(SVGWM_LOAD, 0, reinterpret_cast<LPARAM>(path));
    }

    HRESULT CloseFile()
    {
        return this->SendMessageHresult(SVGWM_CLOSE, 0, 0);
    }

    HRESULT SaveToPNG(LPCWSTR path)
    {
        return this->SendMessageHresult(SVGWM_EXPORT, SVGEXP_PNG, reinterpret_cast<LPARAM>(path));
    }

    HRESULT SetZoomMode(int mode)
    {
        if (mode < 0) {
            return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_ZOOM, mode);
        }
        return E_INVALIDARG;
    }

    HRESULT SetZoomScale(UINT scale)
    {
        return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_ZOOM, scale);
    }

    HRESULT ShowViewBox(bool show)
    {
        return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_VIEWBOX, show);
    }

    HRESULT ShowBoundingBox(bool show)
    {
        return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_BBOX, show);
    }

    HRESULT SetSpeedOverQuality(bool enabled)
    {
        return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_SPEED, enabled);
    }

    void DpiChanged()
    {
        this->SendMessage(SVGWM_DPI_CHANGED);
    }

    void Refresh()
    {
        this->SendMessage(SVGWM_REFRESH);
    }

    void Copy()
    {
        this->SendMessage(WM_COPY);
    }

    bool Create(UINT id, HWND hwndParent)
    {
        HINSTANCE hinstDLL = nullptr;
        ATOM atom = 0;
        if (FAILED(::RegisterViewerClass(&hinstDLL, &atom))) {
            return false;
        }
        return __super::Create(atom, hinstDLL, id, hwndParent);
    }
};

#endif
