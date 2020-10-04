#ifndef SVG_TOOLBAR_H
#define SVG_TOOLBAR_H


#include <new>
#include "window.hpp"

class Toolbar : public Window<Toolbar>
{
private:
#include <pshpack1.h>
    struct ToolbarData
    {
        WORD version;   // = 1
        WORD width;     // not used
        WORD height;    // not used
        WORD count;
        WORD ids[1];    // ids[count]
    };
#include <poppack.h>

    static const int DEFAULT_PADDING = 4;

    static const BITMAPINFOHEADER* GetBitmapResourceInfo(UINT idBmp)
    {
        HRSRC hrsrc = ::FindResourceEx(HINST_THISCOMPONENT, RT_BITMAP, MAKEINTRESOURCE(idBmp), 0);
        if (hrsrc != nullptr) {
            HGLOBAL hglob = ::LoadResource(HINST_THISCOMPONENT, hrsrc);
            if (hglob != nullptr) {
                const BITMAPINFOHEADER* bih = static_cast<LPBITMAPINFOHEADER>(::LockResource(hglob));
                if (bih->biSize == sizeof(BITMAPINFOHEADER) || bih->biSize == sizeof(BITMAPV4HEADER) || bih->biSize == sizeof(BITMAPV5HEADER)) {
                    return bih;
                }
            }
        }
        return nullptr;
    }

    static TBBUTTON* LoadButtons(UINT idToolbar, UINT* pcBtns, UINT* pcBmps)
    {
        UINT cBtns = 0, cBmps = 0;
        TBBUTTON* buttons = nullptr;
        HRSRC hrsrc = ::FindResourceEx(HINST_THISCOMPONENT, RT_TOOLBAR, MAKEINTRESOURCE(idToolbar), 0);
        if (hrsrc != nullptr) {
            HGLOBAL hglob = ::LoadResource(HINST_THISCOMPONENT, hrsrc);
            if (hglob != nullptr) {
                const ToolbarData* data = static_cast<ToolbarData*>(::LockResource(hglob));
                if (data->version == 1 && data->count > 0) {
                    cBtns = data->count;
                    buttons = new (std::nothrow) TBBUTTON[cBtns];
                    if (buttons != nullptr) {
                        ZeroMemory(buttons, sizeof(TBBUTTON) * cBtns);
                        for (UINT i = 0; i < cBtns; i++) {
                            if (data->ids[i] != 0) {
                                buttons[i].iBitmap = cBmps++;
                                buttons[i].idCommand = data->ids[i];
                                buttons[i].fsState = TBSTATE_ENABLED;
                            } else {
                                buttons[i].fsStyle = BTNS_SEP;
                            }
                        }
                    }
                }
            }
        }
        *pcBtns = cBtns;
        *pcBmps = cBmps;
        return buttons;
    }

public:
    Toolbar& SetParent(HWND hwndParent)
    {
        this->SendMessage(TB_SETPARENT, reinterpret_cast<WPARAM>(hwnd));
        return *this;
    }

    Toolbar& SetButtons(UINT idBmp, UINT idToolbar)
    {
        this->ReloadBitmap(idBmp);
        UINT cBtns, cBmps;
        TBBUTTON* buttons = LoadButtons(idToolbar, &cBtns, &cBmps);
        this->SendMessage(TB_ADDBUTTONS, cBtns, reinterpret_cast<LPARAM>(buttons));
        delete[] buttons;
        return *this;
    }

    Toolbar& ReloadBitmap(UINT idBmp)
    {
        const BITMAPINFOHEADER* bih = GetBitmapResourceInfo(idBmp);
        if (bih != nullptr) {
            // Only square images are supported
            const int size = ::abs(bih->biHeight);
            HIMAGELIST himl = ::ImageList_LoadImage(HINST_THISCOMPONENT, MAKEINTRESOURCE(idBmp), size, 1, CLR_NONE, IMAGE_BITMAP, LR_CREATEDIBSECTION);
            himl = reinterpret_cast<HIMAGELIST>(this->SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(himl)));
            if (himl != nullptr) {
                ::ImageList_Destroy(himl);
            }
            this->SendMessage(TB_SETBITMAPSIZE, 0, MAKELPARAM(size, size));
            const int padding = this->DipToPixel(DEFAULT_PADDING);
            this->SendMessage(TB_SETBUTTONSIZE, 0, MAKELPARAM(size + padding, size + padding));
        }
        return *this;
    }

    Toolbar& Enable(UINT id, bool state)
    {
        this->SendMessage(TB_ENABLEBUTTON, id, state);
        return *this;
    }

    Toolbar& Check(UINT id, bool state)
    {
        this->SendMessage(TB_CHECKBUTTON, id, state);
        return *this;
    }

    SIZE GetMaxSize() const
    {
        SIZE size = {};
        this->SendMessageBool(TB_GETMAXSIZE, 0, reinterpret_cast<LPARAM>(&size));
        return size;
    }

    bool Create(UINT id, HWND hwndParent, DWORD style, DWORD exStyle)
    {
        bool result = __super::Create(TOOLBARCLASSNAME, nullptr, id, hwndParent, style);
        if (result) {
            this->SendMessage(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON));
            this->SendMessage(CCM_SETVERSION, 6);
            this->SendMessage(TB_SETEXTENDEDSTYLE, 0, exStyle);
        }
        return result;
    }
};

#endif
