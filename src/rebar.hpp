#ifndef SVG_REBAR_H
#define SVG_REBAR_H


#include "window.hpp"

class ReBar : public Window<ReBar>
{
public:
    bool InsertBand(const REBARBANDINFO& rbbi)
    {
        return this->SendMessageBool(RB_INSERTBAND, ~0U, reinterpret_cast<LPARAM>(&rbbi));
    }

    ReBar& UpdateLayout()
    {
        this->SendMessage(WM_SIZE);
        return *this;
    }

    bool Create(UINT id, HWND hwndParent, DWORD style)
    {
        return __super::Create(REBARCLASSNAME, nullptr, id, hwndParent, style);
    }
};

#endif
