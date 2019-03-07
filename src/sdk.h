#ifndef __IThumbnailProvider_FWD_DEFINED__
#define __IThumbnailProvider_FWD_DEFINED__
typedef interface IThumbnailProvider IThumbnailProvider;
#endif  // __IThumbnailProvider_FWD_DEFINED__

#ifndef __IThumbnailProvider_INTERFACE_DEFINED__
#define __IThumbnailProvider_INTERFACE_DEFINED__

typedef enum WTS_ALPHATYPE {
    WTSAT_UNKNOWN = 0,
    WTSAT_RGB = 1,
    WTSAT_ARGB = 2
} WTS_ALPHATYPE;

EXTERN_C const IID IID_IThumbnailProvider;

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IThumbnailProvider, 0xe357fccd, 0xa995, 0x4576, 0xb0, 0x1f, 0x23, 0x46, 0x30, 0x15, 0x4e, 0x96)
#else
static const IID IID_IThumbnailProvider = { 0xe357fccd, 0xa995, 0x4576, { 0xb0, 0x1f, 0x23, 0x46, 0x30, 0x15, 0x4e, 0x96 } };
#endif  // __CRT_UUID_DECL

#undef INTERFACE
#define INTERFACE IThumbnailProvider

DECLARE_INTERFACE_IID_(IThumbnailProvider, IUnknown, "e357fccd-a995-4576-b01f-234630154e96")
{
    STDMETHOD(GetThumbnail)(THIS_ UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha) PURE;
};

#endif  // __IThumbnailProvider_INTERFACE_DEFINED__
