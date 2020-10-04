#ifndef SVG_VIEWER_API_H
#define SVG_VIEWER_API_H


#include "thumpsvg.h"

#define SVGWM_FIRST             (WM_USER + 0x200)

#define SVGWM_IS_LOADED         (SVGWM_FIRST + 0)
#define SVGWM_IS_EMPTY          (SVGWM_FIRST + 1)
#define SVGWM_GET_SIZE          (SVGWM_FIRST + 2)
#define SVGWM_GET_VIEW_BOX      (SVGWM_FIRST + 3)
#define SVGWM_GET_BOUNDING_BOX  (SVGWM_FIRST + 4)
#define SVGWM_GET_OPTION        (SVGWM_FIRST + 5)
#define SVGWM_SET_OPTION        (SVGWM_FIRST + 6)
#define SVGWM_LOAD              (SVGWM_FIRST + 7)
#define SVGWM_CLOSE             (SVGWM_FIRST + 8)
#define SVGWM_EXPORT            (SVGWM_FIRST + 9)
#define SVGWM_DPI_CHANGED       (SVGWM_FIRST + 10)
#define SVGWM_REFRESH           (SVGWM_FIRST + 11)

// SVGWM_GET_OPTION/SVGWM_SET_OPTION
#define SVGOPT_ZOOM             1
#define SVGOPT_VIEWBOX          2
#define SVGOPT_BBOX             3

// SVGOPT_ZOOM
#define SVGZOOM_CONTAIN         -1
#define SVGZOOM_COVER           -2

// SVGWM_EXPORT
#define SVGEXP_PNG              0
#define SVGEXP_BMP              1
#define SVGEXP_GIF              2


struct SvgSizeU
{
    UINT width;
    UINT height;
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

    SvgSizeU GetSize() const
    {
        SvgSizeU size = {};
        if (SUCCEEDED(this->SendMessageHresult(SVGWM_GET_SIZE, 0, reinterpret_cast<LPARAM>(&size)))) {
            return size;
        }
        SvgSizeU empty = {};
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

    bool ShowViewBox(bool show)
    {
        return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_VIEWBOX, show);
    }

    bool ShowBoundingBox(bool show)
    {
        return this->SendMessageHresult(SVGWM_SET_OPTION, SVGOPT_BBOX, show);
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
