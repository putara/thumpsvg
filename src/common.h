#ifndef SVG_COMMON_H
#define SVG_COMMON_H


// https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483
#if defined(_M_IA64)
#pragma section(".base", long, read)
extern "C"
__declspec(allocate(".base"))
IMAGE_DOS_HEADER __ImageBase;
#else
extern "C"
IMAGE_DOS_HEADER __ImageBase;
#endif

#define HINST_THISCOMPONENT reinterpret_cast<HINSTANCE>(&__ImageBase)

#ifndef RT_TOOLBAR
#define RT_TOOLBAR      MAKEINTRESOURCE(241)
#endif


inline int LoadAppString(UINT id, LPWSTR dst, int cchDst)
{
    int cch = ::LoadStringW(HINST_THISCOMPONENT, id, dst, cchDst);
    if (cch == 0 && cchDst > 0) {
        // null out on failure
        *dst = L'\0';
    }
    return cch;
}

template <int cchDst>
inline int LoadAppString(UINT id, WCHAR (&dst)[cchDst])
{
    return LoadAppString(id, dst, cchDst);
}

inline HICON LoadAppIcon(UINT id, int cx, int cy, UINT flags)
{
    return reinterpret_cast<HICON>(::LoadImageW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(id), IMAGE_ICON, cx, cy, flags));
}

inline HICON LoadAppIcon(LPCWSTR name, int cx, int cy, UINT flags)
{
    return reinterpret_cast<HICON>(::LoadImageW(HINST_THISCOMPONENT, name, IMAGE_ICON, cx, cy, flags));
}

inline HICON LoadAppIcon(UINT id)
{
    return LoadAppIcon(id, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
}

inline HICON LoadAppIcon(LPCWSTR name)
{
    return LoadAppIcon(name, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
}

inline HRESULT ResultFromWin32Error(DWORD err)
{
    return HRESULT_FROM_WIN32(err);
}

inline HRESULT ResultFromLastError()
{
    const DWORD err = ::GetLastError();
    return ResultFromWin32Error(err);
}

inline HRESULT ResultFromKnownLastError()
{
    const DWORD err = ::GetLastError();
    return (err == ERROR_SUCCESS) ? E_FAIL : ResultFromWin32Error(err);
}


// Shut up exceptions
class NoThrowObject
{
public:
    void* operator new(size_t cb) noexcept
    {
        return ::malloc(cb);
    }

    void* operator new[](size_t cb) noexcept
    {
        return ::malloc(cb);
    }

    void operator delete(void* p) noexcept
    {
        ::free(p);
    }

    void operator delete[](void* p) noexcept
    {
        ::free(p);
    }
};

#endif
