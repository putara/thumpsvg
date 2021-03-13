#define _WIN32_WINNT    0x0A00
#define _WIN32_IE       0x0A00
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <intsafe.h>
#include <utility>

#include "bitmap.hpp"
#include "brush.hpp"
#include "rect.hpp"

#include "app.hpp"
#include "winimpl.hpp"
#include "svgview.rc"

#include "common.h"
#include "debug.hpp"
#include "viewer.hpp"
#include "rebar.hpp"
#include "toolbar.hpp"
#include "status.hpp"
#include "uistate.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "thumpsvg.lib")


class SvgWindow : public WindowImpl<SvgWindow>
{
private:
    SvgViewer viewer;
    ReBar rebar;
    Toolbar toolbar;
    StatusBar status;
    UIStateManager manager;

    static const UINT IDC_VIEWER = 1;
    static const UINT IDC_TOOLBAR = 2;
    static const UINT IDC_STATUS = 3;

    class FileDialog
    {
    private:
        OPENFILENAMEW ofn;
        wchar_t path[MAX_PATH];
        wchar_t filter[256];

        template <int cchDst>
        static LPCWSTR LoadFilterString(UINT id, WCHAR (&dst)[cchDst])
        {
            if (LoadAppString(id, dst) > 0) {
                LPWSTR p = dst;
                while ((p = ::wcschr(p, '|')) != nullptr) {
                    *p = L'\0';
                    p++;
                }
                return dst;
            }
            return nullptr;
        }

    public:
        FileDialog(HWND hwnd, DWORD flags, UINT idFilter, LPCWSTR defExt)
        {
            ZeroMemory(&this->ofn, RTL_SIZEOF_THROUGH_FIELD(FileDialog, filter) - FIELD_OFFSET(FileDialog, ofn));
            this->ofn.lStructSize = sizeof(this->ofn);
            this->ofn.hwndOwner = hwnd;
            this->ofn.Flags = flags;
            this->ofn.lpstrFile = this->path;
            this->ofn.nMaxFile = ARRAYSIZE(this->path);
            this->ofn.lpstrFilter = LoadFilterString(idFilter, filter);
            this->ofn.lpstrDefExt = defExt;
        }

        LPCWSTR Open()
        {
            if (::GetOpenFileNameW(&ofn)) {
                return this->path;
            }
            return nullptr;
        }

        LPCWSTR Save()
        {
            if (::GetSaveFileNameW(&ofn)) {
                return this->path;
            }
            return nullptr;
        }
    };

    class ZoomDialog
    {
    private:
        UINT scale;

        INT_PTR DialogProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
        {
            BOOL valid;
            UINT value;
            switch (message) {
            case WM_INITDIALOG:
                ::SetDlgItemInt(hdlg, IDC_ZOOM_PERCENTAGE, this->scale, FALSE);
                ::SendDlgItemMessage(hdlg, IDC_ZOOM_SPIN, UDM_SETRANGE, 0, MAKELPARAM(5000, 1));
                return TRUE;
            case WM_CLOSE:
                ::EndDialog(hdlg, IDCANCEL);
                break;
            case WM_COMMAND:
                switch (GET_WM_COMMAND_ID(wParam, lParam)) {
                case IDOK:
                    valid = FALSE;
                    value = ::GetDlgItemInt(hdlg, IDC_ZOOM_PERCENTAGE, &valid, FALSE);
                    if (valid == FALSE || value == 0) {
                        break;
                    }
                    this->scale = value;
                    // fallthrough
                case IDCANCEL:
                    ::EndDialog(hdlg, GET_WM_COMMAND_ID(wParam, lParam));
                }
                break;
            }
            return FALSE;
        }

        static INT_PTR CALLBACK StaticDialogProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
        {
            auto self = reinterpret_cast<ZoomDialog*>(::GetWindowLongPtr(hdlg, DWLP_USER));
            if (self == nullptr && message == WM_INITDIALOG) {
                ::SetWindowLongPtr(hdlg, DWLP_USER, lParam);
                self = reinterpret_cast<ZoomDialog*>(lParam);
            }
            if (self == nullptr) {
                return FALSE;
            }
            return self->DialogProc(hdlg, message, wParam, lParam);
        }

    public:
        ZoomDialog(UINT scale = 100)
        {
            this->scale = scale;
        }

        UINT Zoom(HWND hwnd)
        {
            if (DialogBoxParam(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDD_ZOOM), hwnd, StaticDialogProc, reinterpret_cast<LPARAM>(this)) == IDOK) {
                return this->scale;
            }
            return 0;
        }
    };

    ULONGLONG GetAppVersion()
    {
#include <pshpack1.h>
        struct VersionResource
        {
            WORD                cbResource;
            WORD                cbBlock;    // = sizeof(VS_FIXEDFILEINFO)
            WORD                type;       // = 0 (binary)
            WCHAR               name[16];   // = L"VS_VERSION_INFO"
            WORD                padding;
            VS_FIXEDFILEINFO    vsffi;
        };
#include <poppack.h>

        HRSRC hrsrc = ::FindResourceEx(HINST_THISCOMPONENT, RT_VERSION, MAKEINTRESOURCE(VS_VERSION_INFO), 0);
        if (hrsrc != nullptr) {
            HGLOBAL hglob = ::LoadResource(HINST_THISCOMPONENT, hrsrc);
            if (hglob != nullptr) {
                DWORD cbRsrc = ::SizeofResource(HINST_THISCOMPONENT, hrsrc);
                if (cbRsrc >= sizeof(VersionResource)) {
                    const VersionResource* verRsrc = static_cast<const VersionResource*>(::LockResource(hglob));
                    if (cbRsrc >= verRsrc->cbResource
                        && verRsrc->cbBlock == sizeof(VS_FIXEDFILEINFO)
                        && verRsrc->type == 0
                        && ::memcmp(verRsrc->name, L"VS_VERSION_INFO", sizeof(L"VS_VERSION_INFO")) == 0
                        && verRsrc->vsffi.dwSignature == VS_FFI_SIGNATURE
                        && verRsrc->vsffi.dwStrucVersion == VS_FFI_STRUCVERSION) {
                        return (static_cast<ULONGLONG>(verRsrc->vsffi.dwFileVersionMS) << 32) | verRsrc->vsffi.dwFileVersionLS;
                    }
                }
            }
        }
        return 0;
    }

public:
    void LoadSVG(_In_ const wchar_t* path)
    {
        this->viewer.LoadFromFile(path);
        this->UpdateTitle(path);
    }

    void UpdateTitle(_In_opt_ const wchar_t* path)
    {
        HWND hwnd = this->GetHwnd();

        const UINT loaded = this->viewer.IsLoaded();
        const UINT notEmpty = this->viewer.IsEmpty() == false;
        this->manager
            .Enable(ID_FILE_SAVE, loaded)
            .Enable(ID_FILE_PROPERTIES, loaded)
            .Enable(ID_EDIT_COPY, notEmpty);

        wchar_t title[64];
        LoadAppString(IDR_MAIN, title);
        this->status.SetText(0, IDS_READY);
        if (path == nullptr || this->viewer.IsLoaded() == false) {
            ::SetWindowTextW(hwnd, title);
            this->status.SetText(1, nullptr);
            return;
        }
        const wchar_t* fileName = ::PathFindFileNameW(path);
        wchar_t text[MAX_PATH + 192];
        ::StringCchPrintfW(text, ARRAYSIZE(text), L"%s - %s", fileName, title);
        ::SetWindowTextW(hwnd, text);
        auto size = this->viewer.GetSize();
        ::StringCchPrintfW(text, ARRAYSIZE(text), L"%g x %g", ::floor(size.width), ::floor(size.height));
        this->status.SetText(1, text);
    }

    void UpdateLayout(HWND hwnd)
    {
        this->rebar.UpdateLayout();
        this->status.UpdateLayout();
        Rect toolbarRect = this->rebar.GetRelativeWindowRect(hwnd);
        Rect statusRect = this->status.GetRelativeWindowRect(hwnd);
        ClientRect rect(hwnd);
        if (this->rebar.IsVisible()) {
            rect.top = toolbarRect.bottom;
        }
        if (this->status.IsVisible()) {
            rect.bottom = statusRect.top;
        }
        this->viewer.Resize(rect);
    }

    BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
    {
        if (this->viewer.Create(IDC_VIEWER, hwnd) == false) {
            return FALSE;
        }

        const DWORD RBS_SVG_DEFAULT
            = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
            | CCS_TOP | CCS_NODIVIDER | RBS_VARHEIGHT | RBS_BANDBORDERS | RBS_AUTOSIZE;
        if (this->rebar.Create(0, hwnd, RBS_SVG_DEFAULT) == false) {
            return FALSE;
        }

        const DWORD TBSTYLE_SVG_DEFAULT
            = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
            | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN
            | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT;
        const DWORD TBSYLE_EX_SVG_DEFAULT
            = TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER;
        if (this->toolbar.Create(IDC_TOOLBAR, this->rebar.GetHwnd(), TBSTYLE_SVG_DEFAULT, TBSYLE_EX_SVG_DEFAULT) == false) {
            return FALSE;
        }

        const DWORD SBARS_SVG_DEFAULT
            = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SBARS_SIZEGRIP;
        if (this->status.Create(IDC_STATUS, hwnd, SBARS_SVG_DEFAULT) == false) {
            return FALSE;
        }

        SIZE size = this->toolbar
            .SetButtons(::GetDpiForWindow(hwnd) >= 144 ? IDB_TOOLBAR32 : IDB_TOOLBAR16, IDR_MAIN)
            .GetMaxSize();
        REBARBANDINFO rbbi = { sizeof(rbbi) };
        rbbi.fMask      = RBBIM_CHILD | RBBIM_STYLE | RBBIM_CHILDSIZE;
        rbbi.fStyle     = RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS | RBBS_USECHEVRON | RBBS_HIDETITLE;
        rbbi.hwndChild  = this->toolbar.GetHwnd();
        rbbi.cyMinChild = size.cy;
        this->rebar.InsertBand(rbbi);

        const UINT parts[] = { StatusBar::SPRING, 6 };
        this->status.SetParts(parts, ARRAYSIZE(parts));

        this->manager
            .Attach(hwnd)
            .AttachMenu(hwnd)
            .AttachToolbar(this->toolbar)
            .CheckIf(ID_VIEW_TOOLBAR, this->toolbar.GetHwnd())
            .CheckIf(ID_VIEW_STATUS, this->status.GetHwnd())
            .Enable(ID_FILE_SAVE, false)
            .Enable(ID_FILE_PROPERTIES, false)
            .Enable(ID_EDIT_COPY, false)
            .Radio(ID_VIEW_ZOOM_FIT, ID_VIEW_ZOOM_START, ID_VIEW_ZOOM_END);

        this->UpdateTitle(nullptr);
        return __super::OnCreate(hwnd, lpcs);
    }

    void OnDestroy(HWND hwnd)
    {
        this->viewer.Detach();
        ::PostQuitMessage(0);
        __super::OnDestroy(hwnd);
    }

    void OnSize(HWND hwnd, UINT state, int cx, int cy)
    {
        this->UpdateLayout(hwnd);
    }

    void OnDropFiles(HWND hwnd, HDROP hdrop)
    {
        WCHAR path[MAX_PATH];
        UINT result = ::DragQueryFileW(hdrop, 0, path, ARRAYSIZE(path));
        ::DragFinish(hdrop);
        if (result) {
            this->LoadSVG(path);
        }
    }

    LRESULT OnNotify(HWND hwnd, int idFrom, NMHDR* pnmhdr)
    {
        if (pnmhdr == nullptr) {
            return 0;
        }
        switch (pnmhdr->code) {
        case TTN_GETDISPINFO:
            return this->OnNotifyTooltipGetDispInfo(CONTAINING_RECORD(pnmhdr, NMTTDISPINFO, hdr));
        }
        return 0;
    }

    LRESULT OnNotifyTooltipGetDispInfo(LPNMTTDISPINFO pnmttdi)
    {
        if ((pnmttdi->uFlags & TTF_IDISHWND) == 0) {
            if (LoadAppString(static_cast<UINT>(pnmttdi->hdr.idFrom), pnmttdi->szText)) {
                pnmttdi->uFlags |= TTF_DI_SETITEM;
            }
        }
        return 0;
    }

    void OnCommandNew(HWND)
    {
        if (this->viewer.IsLoaded()) {
            this->viewer.CloseFile();
            this->UpdateTitle(nullptr);
        }
    }

    void OnCommandOpen(HWND hwnd)
    {
        FileDialog dlg(hwnd, OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING, IDS_DLG_FILTER_OPEN, L".svg");
        LPCWSTR path = dlg.Open();
        if (path != nullptr) {
            this->LoadSVG(path);
        }
    }

    void OnCommandSave(HWND hwnd)
    {
        if (this->viewer.IsLoaded()) {
            FileDialog dlg(hwnd, OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING, IDS_DLG_FILTER_SAVE, L".png");
            LPCWSTR path = dlg.Save();
            if (path != nullptr) {
                HRESULT hr = this->viewer.SaveToPNG(path);
                if (FAILED(hr)) {
                    wchar_t error[128];
                    StringCchPrintfW(error, ARRAYSIZE(error), L"Something's wrong. HResult = 0x%08lx (%s)", hr, GetErrorCode(hr));
                    ::MessageBoxW(hwnd, error, nullptr, MB_ICONEXCLAMATION);
                }
            }
        }
    }

    void OnCommandProperties(HWND hwnd)
    {
        if (this->viewer.IsLoaded()) {
            SvgSizeF size = this->viewer.GetSize();
            SvgRectF vrect = this->viewer.GetViewBox();
            wchar_t text[256];
            ::StringCchPrintfW(text, ARRAYSIZE(text),
                L"Dimensions: %g x %g\n"
                L"View box: (%g, %g) - (%g, %g) %g x %g\n",
                size.width, size.height,
                vrect.x, vrect.y, vrect.x + vrect.width, vrect.y + vrect.height, vrect.width, vrect.height);
            SvgRectF brect = {};
            if (this->viewer.GetBoundingBox(&brect) == S_OK) {
                size_t len = 0;
                ::StringCchLengthW(text, ARRAYSIZE(text), &len);
                ::StringCchPrintfW(text + len, ARRAYSIZE(text) - len,
                    L"Bounding box: (%g, %g) - (%g, %g) %g x %g\n",
                    brect.x, brect.y, brect.x + brect.width, brect.y + brect.height, brect.width, brect.height);
            }
            wchar_t title[64];
            LoadAppString(IDR_MAIN, title);
            ::MessageBoxW(hwnd, text, title, MB_OK);
        }
    }

    void OnCommandExit(HWND hwnd)
    {
        FORWARD_WM_CLOSE(hwnd, ::PostMessage);
    }

    void OnCommandCopy(HWND hwnd)
    {
        this->viewer.Copy();
    }

    void OnCommandAbout(HWND hwnd)
    {
        ULONGLONG version = GetAppVersion();
        wchar_t format[128], title[256];
        LoadAppString(IDS_APP_VERSION, format);
        ::StringCchPrintfW(title, ARRAYSIZE(title), format, LOWORD(version >> 48), LOWORD(version >> 32), LOWORD(version >> 16), LOWORD(version));
        TASKDIALOGCONFIG tdc = { sizeof(tdc) };
        tdc.hwndParent = hwnd;
        tdc.hInstance = HINST_THISCOMPONENT;
        tdc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
        tdc.pszWindowTitle = MAKEINTRESOURCEW(IDR_MAIN);
        tdc.pszMainIcon = MAKEINTRESOURCEW(IDR_MAIN);
        tdc.pszMainInstruction = title;
        tdc.pszContent = MAKEINTRESOURCEW(IDS_APP_LINKS);
        tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_ENABLE_HYPERLINKS;
        tdc.pfCallback = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR refData) -> HRESULT {
            if (msg == TDN_HYPERLINK_CLICKED) {
                LPCWSTR link = reinterpret_cast<LPCWSTR>(lParam);
                LPCWSTR url = nullptr;
                if (::wcscmp(link, L"home") == 0) {
                    url = L"https://github.com/putara/thumpsvg";
                } else if (::wcscmp(link, L"resvg") == 0) {
                    url = L"https://github.com/RazrFalcon/resvg";
                } else if (::wcscmp(link, L"tinyskia") == 0) {
                    url = L"https://github.com/RazrFalcon/tiny-skia";
                } else if (::wcscmp(link, L"fatcow") == 0) {
                    url = L"https://www.fatcow.com/free-icons";
                } else if (::wcscmp(link, L"w3c") == 0) {
                    url = L"https://www.w3.org/Graphics/SVG/";
                }
                if (url != nullptr) {
                    ::ShellExecuteW(hwnd, nullptr, url, nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        };
        ::TaskDialogIndirect(&tdc, nullptr, nullptr, nullptr);
    }

    void OnCommandToolbar(HWND hwnd)
    {
        this->rebar.ShowHide();
        this->UpdateLayout(hwnd);
    }

    void OnCommandStatus(HWND hwnd)
    {
        this->status.ShowHide();
        this->UpdateLayout(hwnd);
    }

    void OnCommandRefresh(HWND hwnd)
    {
        this->viewer.Refresh();
    }

    void OnCommandViewBox(HWND hwnd)
    {
        bool shown = true;
        HRESULT hr = this->viewer.ShowViewBox(true);
        if (hr == S_OK) {
            shown = false;
            this->viewer.ShowViewBox(false);
        }
        this->manager.Check(ID_VIEW_VIEWBOX, shown);
    }

    void OnCommandBBox(HWND hwnd)
    {
        bool shown = true;
        HRESULT hr = this->viewer.ShowBoundingBox(true);
        if (hr == S_OK) {
            shown = false;
            this->viewer.ShowBoundingBox(false);
        }
        this->manager.Check(ID_VIEW_BBOX, shown);
    }

    void OnCommandZoomFit(HWND hwnd)
    {
        this->viewer.SetZoomMode(SVGZOOM_CONTAIN);
        this->manager.Radio(ID_VIEW_ZOOM_FIT, ID_VIEW_ZOOM_START, ID_VIEW_ZOOM_END);
    }

    void OnCommandZoomOriginal(HWND hwnd)
    {
        this->viewer.SetZoomScale(100);
        this->manager.Radio(ID_VIEW_ZOOM_ORIGINAL, ID_VIEW_ZOOM_START, ID_VIEW_ZOOM_END);
    }

    void OnCommandZoomCustom(HWND hwnd)
    {
        ZoomDialog dlg;
        UINT scale = dlg.Zoom(hwnd);
        if (scale) {
            this->viewer.SetZoomScale(scale);
            this->manager.Radio(scale == 100 ? ID_VIEW_ZOOM_ORIGINAL : ID_VIEW_ZOOM_CUSTOM, ID_VIEW_ZOOM_START, ID_VIEW_ZOOM_END);
        }
    }

    void OnCommand(HWND hwnd, int id, HWND, UINT)
    {
        switch (id) {
        case ID_FILE_NEW:
            OnCommandNew(hwnd);
            break;
        case ID_FILE_OPEN:
            OnCommandOpen(hwnd);
            break;
        case ID_FILE_SAVE:
            OnCommandSave(hwnd);
            break;
        case ID_FILE_PROPERTIES:
            OnCommandProperties(hwnd);
            break;
        case ID_FILE_EXIT:
            OnCommandExit(hwnd);
            break;
        case ID_EDIT_COPY:
            OnCommandCopy(hwnd);
            break;
        case ID_HELP_ABOUT:
            OnCommandAbout(hwnd);
            break;
        case ID_VIEW_TOOLBAR:
            OnCommandToolbar(hwnd);
            break;
        case ID_VIEW_STATUS:
            OnCommandStatus(hwnd);
            break;
        case ID_VIEW_REFRESH:
            OnCommandRefresh(hwnd);
            break;
        case ID_VIEW_VIEWBOX:
            OnCommandViewBox(hwnd);
            break;
        case ID_VIEW_BBOX:
            OnCommandBBox(hwnd);
            break;
        case ID_VIEW_ZOOM_FIT:
            OnCommandZoomFit(hwnd);
            break;
        case ID_VIEW_ZOOM_ORIGINAL:
            OnCommandZoomOriginal(hwnd);
            break;
        case ID_VIEW_ZOOM_CUSTOM:
            OnCommandZoomCustom(hwnd);
            break;
        case ID_VIEW_ZOOM_IN:
        case ID_VIEW_ZOOM_OUT:
        case ID_TOOL_OPTIONS:
            MessageBoxW(hwnd, L"Not yet implemented", L"Sorry", MB_OK);
            break;
        }
    }

    void OnInitMenuPopup(HWND hwnd, HMENU, UINT, BOOL sysMenu)
    {
        if (sysMenu) {
            this->status.Simple(true);
        }
    }

    void OnUninitMenuPopup(HWND hwnd)
    {
        this->status.Simple(false);
    }

    void OnMenuSelect(HWND hwnd, WPARAM wParam, LPARAM lParam)
    {
        if (GET_WM_MENUSELECT_HMENU(wParam, lParam) == nullptr && GET_WM_MENUSELECT_FLAGS(wParam, lParam) == -1) {
            // Menu is closed
        } else if (GET_WM_MENUSELECT_FLAGS(wParam, lParam) & MF_SYSMENU) {
            const UINT item = GET_WM_MENUSELECT_CMD(wParam, lParam);
            if (item >= SC_SIZE) {
                // Call out an obsolete function to fill in the status bar.
                ::MenuHelp(WM_MENUSELECT, wParam, lParam, nullptr, nullptr, this->status.GetHwnd(), nullptr);
            }
        }
    }

    static LPCWSTR GetErrorCode(HRESULT hr)
    {
#define case_hr(x) case (x): return L ## #x
        switch (hr) {
        case_hr(S_OK);
        case_hr(E_FAIL);
        case_hr(E_INVALIDARG);
        case_hr(E_OUTOFMEMORY);
        case_hr(E_NOTIMPL);
        case_hr(E_ABORT);
        case_hr(E_ACCESSDENIED);
        case_hr(INTSAFE_E_ARITHMETIC_OVERFLOW);
        case_hr(WINCODEC_ERR_WRONGSTATE);
        case_hr(WINCODEC_ERR_VALUEOUTOFRANGE);
        case_hr(WINCODEC_ERR_UNKNOWNIMAGEFORMAT);
        case_hr(WINCODEC_ERR_UNSUPPORTEDVERSION);
        case_hr(WINCODEC_ERR_NOTINITIALIZED);
        case_hr(WINCODEC_ERR_ALREADYLOCKED);
        case_hr(WINCODEC_ERR_PROPERTYNOTFOUND);
        case_hr(WINCODEC_ERR_PROPERTYNOTSUPPORTED);
        case_hr(WINCODEC_ERR_PROPERTYSIZE);
        case_hr(WINCODEC_ERR_CODECPRESENT);
        case_hr(WINCODEC_ERR_CODECNOTHUMBNAIL);
        case_hr(WINCODEC_ERR_PALETTEUNAVAILABLE);
        case_hr(WINCODEC_ERR_CODECTOOMANYSCANLINES);
        case_hr(WINCODEC_ERR_INTERNALERROR);
        case_hr(WINCODEC_ERR_SOURCERECTDOESNOTMATCHDIMENSIONS);
        case_hr(WINCODEC_ERR_COMPONENTNOTFOUND);
        case_hr(WINCODEC_ERR_IMAGESIZEOUTOFRANGE);
        case_hr(WINCODEC_ERR_TOOMUCHMETADATA);
        case_hr(WINCODEC_ERR_BADIMAGE);
        case_hr(WINCODEC_ERR_BADHEADER);
        case_hr(WINCODEC_ERR_FRAMEMISSING);
        case_hr(WINCODEC_ERR_BADMETADATAHEADER);
        case_hr(WINCODEC_ERR_BADSTREAMDATA);
        case_hr(WINCODEC_ERR_STREAMWRITE);
        case_hr(WINCODEC_ERR_STREAMREAD);
        case_hr(WINCODEC_ERR_STREAMNOTAVAILABLE);
        case_hr(WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT);
        case_hr(WINCODEC_ERR_UNSUPPORTEDOPERATION);
        case_hr(WINCODEC_ERR_INVALIDREGISTRATION);
        case_hr(WINCODEC_ERR_COMPONENTINITIALIZEFAILURE);
        case_hr(WINCODEC_ERR_INSUFFICIENTBUFFER);
        case_hr(WINCODEC_ERR_DUPLICATEMETADATAPRESENT);
        case_hr(WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE);
        case_hr(WINCODEC_ERR_UNEXPECTEDSIZE);
        case_hr(WINCODEC_ERR_INVALIDQUERYREQUEST);
        case_hr(WINCODEC_ERR_UNEXPECTEDMETADATATYPE);
        case_hr(WINCODEC_ERR_REQUESTONLYVALIDATMETADATAROOT);
        case_hr(WINCODEC_ERR_INVALIDQUERYCHARACTER);
        case_hr(WINCODEC_ERR_WIN32ERROR);
        case_hr(WINCODEC_ERR_INVALIDPROGRESSIVELEVEL);
        case_hr(WINCODEC_ERR_INVALIDJPEGSCANINDEX);
        }
#undef case_hr
        return L"\?\?\?";
    }

    void OnContextMenu(HWND hwnd, HWND hwndContext, int xPos, int yPos)
    {
        if (hwndContext != this->viewer.GetHwnd()) {
            FORWARD_WM_CONTEXTMENU(hwnd, hwndContext, xPos, yPos, __super::WindowProc);
            return;
        }
        if (xPos == -1 && yPos == -1) {
            ClientRect rect(hwnd);
            auto point = rect.CenterPoint();
            xPos = point.x;
            yPos = point.y;
        }
        // HMENU hm = ::CreatePopupMenu();
        // ::AppendMenuW(hm, MF_STRING, 1, L"&Save");
        // const UINT idCmd = ::TrackPopupMenu(hm, TPM_RIGHTBUTTON | TPM_RETURNCMD, xPos, yPos, 0, hwnd, nullptr);
        // ::DestroyMenu(hm);
        // if (idCmd != 1) {
        //     return;
        // }
    }

    void OnDpiChanged(HWND hwnd, int dpiX, int dpiY, const RECT* newScaleRect)
    {
        this->viewer.DpiChanged();
        Rect rect(*newScaleRect);
        ::SetWindowPos(hwnd, nullptr, rect.left, rect.top,
            rect.width(), rect.height(),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result;
        if (this->manager.HandleWindowMessage(hwnd, message, wParam, lParam, &result)) {
            return result;
        }
        if (message == WM_NCCREATE) {
            ::EnableNonClientDpiScaling(hwnd);
        }
        switch (message) {
        HANDLE_MSG(hwnd, WM_SIZE, this->OnSize);
        HANDLE_MSG(hwnd, WM_DROPFILES, this->OnDropFiles);
        HANDLE_MSG(hwnd, WM_NOTIFY, this->OnNotify);
        HANDLE_MSG(hwnd, WM_COMMAND, this->OnCommand);
        HANDLE_MSG(hwnd, WM_INITMENUPOPUP, this->OnInitMenuPopup);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, this->OnContextMenu);
        case WM_UNINITMENUPOPUP:
            return this->OnUninitMenuPopup(hwnd), 0L;
        case WM_MENUSELECT:
            return this->OnMenuSelect(hwnd, wParam, lParam), 0L;
        case WM_DPICHANGED:
            return this->OnDpiChanged(hwnd, HIWORD(wParam), LOWORD(wParam), reinterpret_cast<const RECT*>(lParam)), 0L;
        }
        return __super::WindowProc(hwnd, message, wParam, lParam);
    }

    static LPCTSTR GetClassName()
    {
        return TEXT("SvgViewerWndClass");
    }

    static WNDCLASSEX GetWindowClass(HINSTANCE hinst)
    {
        WNDCLASSEX wc = __super::GetWindowClass(hinst);
        wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAIN);
        wc.hIcon = LoadAppIcon(IDR_MAIN);
        return wc;
    }
};


class SvgApp : public App<SvgApp, SvgWindow>
{
private:
    int argc;
    LPCWSTR const* argv;
    HACCEL hacc = nullptr;

public:
    SvgApp(int argc, LPCWSTR const* argv)
    {
        this->argc = argc;
        this->argv = argv;
    }

    SvgWindow* CreateMainWindow(HINSTANCE hinst, ATOM atom)
    {
        this->hacc = ::LoadAccelerators(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDR_MAIN));
        DPI_AWARENESS_CONTEXT oldContext = ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (oldContext == 0) {
            oldContext = ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
        }
        SvgWindow* self = SvgWindow::Create(hinst, atom, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, WS_OVERLAPPEDWINDOW, WS_EX_ACCEPTFILES);
        if (oldContext != 0) {
            ::SetThreadDpiAwarenessContext(oldContext);
        }
        return self;
    }

    void OnMainWindowCreated(SvgWindow* window)
    {
        if (this->argc > 1) {
            window->LoadSVG(this->argv[1]);
        }
    }

    bool OnProcessMessage(HWND hwnd, MSG& msg)
    {
        if (this->hacc != nullptr && (hwnd == msg.hwnd || ::IsChild(hwnd, msg.hwnd))) {
            if (::TranslateAccelerator(hwnd, this->hacc, &msg)) {
                return true;
            }
        }
        return false;
    }
};


int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_COOL_CLASSES };
    ::InitCommonControlsEx(&icc);
    int argc = 0;
    auto argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return 1;
    }

    SvgApp app(argc, argv);
    return app.Main(hinst);
}
