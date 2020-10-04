#ifndef SVG_STATUSBAR_H
#define SVG_STATUSBAR_H


#include <new>
#include <algorithm>
#include "window.hpp"

class StatusBar : public Window<StatusBar>
{
private:
    UINT cParts = 0;
    UINT* parts = nullptr;

    int CalcBaseUnit() const
    {
        int baseUnit = 0;
        HDC hdc = ::GetDC(this->hwnd);
        if (hdc != nullptr) {
            Rect rect = {};
            HFONT oldfont = SelectFont(hdc, GetWindowFont(this->hwnd));
            ::DrawText(hdc, TEXT("0x0"), -1, &rect, DT_CALCRECT | DT_SINGLELINE);
            SelectFont(hdc, oldfont);
            ::ReleaseDC(this->hwnd, hdc);
            baseUnit = rect.width();
        }
        if (baseUnit <= 0) {
            baseUnit = 8;
        }
        return baseUnit;
    }

public:
    static const UINT SPRING = ~0U;

    StatusBar()
    {
    }

    ~StatusBar()
    {
        delete[] this->parts;
    }

    StatusBar& Simple(bool simple)
    {
        if (simple) {
            this->SendMessage(SB_SIMPLE, TRUE);
            this->SendMessage(SB_SETTEXT, 255 | SBT_NOBORDERS);
        } else {
            this->SendMessage(SB_SIMPLE, FALSE);
        }
        return *this;
    }

    StatusBar& SetParts(const UINT* parts, UINT cParts)
    {
        delete[] this->parts;
        this->cParts = 0;
        this->parts = nullptr;
        if (cParts > 0) {
            this->parts = new (std::nothrow) UINT[cParts];
            if (this->parts != nullptr) {
                this->cParts = cParts;
                std::copy_n(parts, cParts, this->parts);
            }
        }
        this->UpdateLayout();
        return *this;
    }

    StatusBar& SetText(UINT part, UINT idText)
    {
        wchar_t text[256];
        if (::LoadString(HINST_THISCOMPONENT, idText, text, ARRAYSIZE(text)) <= 0) {
            *text = 0;
        }
        return this->SetText(part, text);
    }

    StatusBar& SetText(UINT part, LPCWSTR text)
    {
        this->SendMessage(SB_SETTEXTW, part, reinterpret_cast<LPARAM>(text));
        return *this;
    }

    StatusBar& UpdateLayout()
    {
        this->SendMessage(WM_SIZE);
        if (this->parts == nullptr || this->cParts == 0) {
            return *this;
        }
        ClientRect rect(this->hwnd);
        int baseUnit = this->CalcBaseUnit();
        int* parts = new (std::nothrow) int[this->cParts];
        if (parts != nullptr) {
            int i = 0;
            const int cParts = this->cParts;
            int right = 0;
            do {
                if (this->parts[i] == SPRING) {
                    break;
                }
                right += baseUnit * this->parts[i];
                parts[i] = right;
            } while (++i < cParts);

            if (i < cParts) {
                right = rect.right;
                for (int j = cParts - 1; j >= i; j--) {
                    parts[j] = right;
                    right -= baseUnit * this->parts[j];
                }
            }
            this->SendMessage(SB_SETPARTS, cParts, reinterpret_cast<LPARAM>(parts));
            delete[] parts;
        }
        return *this;
    }

    bool Create(UINT id, HWND hwndParent, DWORD style)
    {
        return __super::Create(STATUSCLASSNAME, nullptr, id, hwndParent, style);
    }
};

#endif
