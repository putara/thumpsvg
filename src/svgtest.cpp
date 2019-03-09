#define WINVER          0x600
#define _WIN32_WINNT    0x600
#define _WIN32_IE       0x0700

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include "sdk.h"

#include <stdio.h>

typedef HRESULT (STDAPICALLTYPE* DLLGETCLASSOBJECT)(REFCLSID, REFIID, void**);

const GUID CLSID_ThumbProviderSVG = { 0xc9148b50, 0xe15e, 0x440d, { 0xa0, 0x7f, 0xcf, 0x38, 0x5e, 0xc7, 0x39, 0x14 } };


HRESULT CreateBindCtxWithMode(DWORD grfMode, IBindCtx** ppbc)
{
    HRESULT hr = ::CreateBindCtx(0, ppbc);
    if (SUCCEEDED(hr)) {
        BIND_OPTS opts = { sizeof(opts), 0, grfMode, 0 };
        (*ppbc)->SetBindOptions(&opts);
    }
    return hr;
}

HRESULT GrabFilesAsShellItemArray(const wchar_t* dir, IShellItemArray** array)
{
    *array = NULL;
    IShellItem* item;
    HRESULT hr = ::SHCreateItemFromParsingName(dir, NULL, IID_PPV_ARGS(&item));
    if (FAILED(hr)) {
        printf("SHCreateItemFromParsingName() failed with 0x%08x\n", hr);
        return hr;
    }
    INamespaceWalk* walk = NULL;
    hr = ::CoCreateInstance(CLSID_NamespaceWalker, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&walk));
    if (SUCCEEDED(hr)) {
        hr = walk->Walk(item, NSWF_DONT_RESOLVE_LINKS | NSWF_NONE_IMPLIES_ALL | NSWF_FILESYSTEM_ONLY, MAXLONG, NULL);
        if (SUCCEEDED(hr)) {
            UINT cItems = 0;
            PIDLIST_ABSOLUTE* ppidls = NULL;
            hr = walk->GetIDArrayResult(&cItems, &ppidls);
            if (SUCCEEDED(hr)) {
                hr = ::SHCreateShellItemArrayFromIDLists(cItems, const_cast<PCIDLIST_ABSOLUTE_ARRAY>(ppidls), array);
                if (FAILED(hr)) {
                    printf("SHCreateShellItemArrayFromIDLists() failed with 0x%08x\n", hr);
                }
                FreeIDListArrayFull(ppidls, cItems);
            } else {
                printf("INamespaceWalk::GetIDArrayResult() failed with 0x%08x\n", hr);
            }
        } else {
            printf("INamespaceWalk::Walk() failed with 0x%08x\n", hr);
        }
        walk->Release();
    } else {
        printf("CoCreateInstance(CLSID_NamespaceWalker) failed with 0x%08x\n", hr);
    }
    item->Release();
    return hr;
}

HRESULT OpenStreamAsReadonly(IShellItem* item, IStream** stream)
{
    *stream = NULL;
    IBindCtx* ctx = NULL;
    HRESULT hr = CreateBindCtxWithMode(STGM_READ, &ctx);
    if (FAILED(hr)) {
        printf("CreateBindCtx() failed with 0x%08x\n", hr);
        return hr;
    }
    hr = item->BindToHandler(ctx, BHID_Stream, IID_PPV_ARGS(stream));
    if (FAILED(hr)) {
        printf("IShellItem::BindToHandler() failed with 0x%08x\n", hr);
    }
    ctx->Release();
    return hr;
}

void runfile(DLLGETCLASSOBJECT dllGetClassObject, IShellItem* item, int size)
{
    wchar_t* path = NULL;
    HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
    if (FAILED(hr)) {
        return;
    }
    wchar_t* ext = ::PathFindExtensionW(path);
    if (::StrCmpICW(ext, L".svg") != 0 && ::StrCmpICW(ext, L".svgz") != 0) {
        ::CoTaskMemFree(path);
        return;
    }

    IStream* stream = NULL;
    hr = OpenStreamAsReadonly(item, &stream);
    if (FAILED(hr)) {
        ::CoTaskMemFree(path);
        return;
    }
    IClassFactory* factory = NULL;
    hr = dllGetClassObject(CLSID_ThumbProviderSVG, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        printf("DllGetClassObject() failed with 0x%08x\n", hr);
        stream->Release();
        ::CoTaskMemFree(path);
        return;
    }
    IThumbnailProvider* thump = NULL;
    hr = factory->CreateInstance(NULL, IID_PPV_ARGS(&thump));
    if (SUCCEEDED(hr)) {
        IInitializeWithStream* init = NULL;
        hr = thump->QueryInterface(IID_PPV_ARGS(&init));
        if (SUCCEEDED(hr)) {
            hr = init->Initialize(stream, STGM_READ);
            if (SUCCEEDED(hr)) {
                HBITMAP hbmp = NULL;
                WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
                hr = thump->GetThumbnail(static_cast<UINT>(size), &hbmp, &alpha);
                if (SUCCEEDED(hr)) {
                    if (hbmp != NULL) {
                        ::DeleteObject(hbmp);
                    } else {
                        printf("%S: IThumbnailProvider::GetThumbnail() succeeded but hbmp = NULL\n", path);
                    }
                    if (alpha == WTSAT_UNKNOWN) {
                        printf("%S: IThumbnailProvider::GetThumbnail() succeeded but alpha = UNKNOWN\n", path);
                    }
                } else {
                    printf("%S: IThumbnailProvider::GetThumbnail() failed with 0x%08x\n", path, hr);
                }
            } else {
                printf("%S: IInitializeWithStream::Initialize() failed with 0x%08x\n", path, hr);
            }
            init->Release();
        } else {
            printf("IThumbnailProvider::QueryInterface() failed with 0x%08x\n", hr);
        }
        thump->Release();
    } else {
        printf("IClassFactory::CreateInstance() failed with 0x%08x\n", hr);
    }
    factory->Release();
    stream->Release();
    ::CoTaskMemFree(path);
}

void run(DLLGETCLASSOBJECT dllGetClassObject, const wchar_t* dir, int size)
{
    printf("Scanning '%S' ...\n", dir);
    IShellItemArray* array = NULL;
    HRESULT hr = GrabFilesAsShellItemArray(dir, &array);
    if (SUCCEEDED(hr)) {
        DWORD count = 0;
        array->GetCount(&count);
        IShellItem* item = NULL;
        for (DWORD i = 0; i < count && SUCCEEDED(array->GetItemAt(i, &item)); i++) {
            runfile(dllGetClassObject, item, size);
            item->Release();
        }
        array->Release();
    }
}

int main()
{
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

    wchar_t path[MAX_PATH];
    HRESULT hr = ::OleInitialize(NULL);
    if (FAILED(hr)) {
        printf("OleInitialize() failed with 0x%08x\n", hr);
        return 1;
    }

    ::GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
    ::PathAppendW(path, L"..\\thumpsvg.dll");

    HMODULE hmod = ::LoadLibraryW(path);
    if (hmod == NULL) {
        printf("LoadLibrary() failed with %u\n", ::GetLastError());
        ::OleUninitialize();
        return 1;
    }

    DLLGETCLASSOBJECT dllGetClassObject = reinterpret_cast<DLLGETCLASSOBJECT>(::GetProcAddress(hmod, "DllGetClassObject"));
    if (dllGetClassObject == NULL) {
        printf("LoadLibrary() failed with %u\n", ::GetLastError());
        ::FreeLibrary(hmod);
        ::OleUninitialize();
        return 1;
    }

    ::GetFullPathNameW(argc < 2 ? L"." : argv[1], ARRAYSIZE(path), path, NULL);
    int size = argc < 3 ? 128 : ::StrToIntW(argv[1]);
    if (size <= 0 || size > 256) {
        size = 128;
    }
    run(dllGetClassObject, path, size);

    ::FreeLibrary(hmod);
    ::OleUninitialize();
    return 0;
}
