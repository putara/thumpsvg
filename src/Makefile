!if "$(target)" == "x86" || "$(target)" == "x32" || "$(target)" == "32"
BITS = 32
ARCH = x86
!else
BITS = 64
ARCH = x64
!endif

INCDIR = ..\build\$(BITS)\inc
LIBDIR = ..\build\$(BITS)\lib
OBJDIR = ..\build\$(BITS)\obj
CDEFRT = SVG_RT_SKIA
SUFFIX =

OUTDIR = ..\bin\$(BITS)

SHLEXT_DLL = $(OUTDIR)\thumpsvg$(SUFFIX).dll
SHLEXT_MAP = $(OUTDIR)\thumpsvg$(SUFFIX).map
SHLEXT_PDB = $(OUTDIR)\thumpsvg$(SUFFIX).pdb
SHLEXT_OBJS = \
 "$(OBJDIR)\thumpsvg.obj"\
 "$(OBJDIR)\thumpsvg.res"

VIEWER_EXE = $(OUTDIR)\svgview$(SUFFIX).exe
VIEWER_MAP = $(OUTDIR)\svgview$(SUFFIX).map
VIEWER_PDB = $(OUTDIR)\svgview$(SUFFIX).pdb
VIEWER_OBJS = \
 "$(OBJDIR)\svgview.obj"\
 "$(OBJDIR)\svgview.res"

CC = cl.exe
LD = link.exe
RC = rc.exe

CFLAGS = /nologo /c /GF /GL /GR- /GS- /Gy /MD /O1ib2 /W3 /Zi /FAcs /Fa"$(OBJDIR)/" /Fo"$(OBJDIR)/" /Fd"$(OBJDIR)/"
LDFLAGS = /nologo /time /dynamicbase:no /ltcg /machine:$(ARCH) /nxcompat /opt:icf /opt:ref /release /debug /pdbaltpath:"%_PDB%"
RFLAGS = /l 0 /d "NDEBUG" /d "$(CDEFRT)"

CDEFS = /D "NDEBUG" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "$(CDEFRT)" /I "$(INCDIR)"
LDLIBS = kernel32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib shlwapi.lib ole32.lib windowscodecs.lib uxtheme.lib dwmapi.lib ws2_32.lib advapi32.lib userenv.lib pathcch.lib

!ifdef debug
!if $(debug)
CDEFS = /D "DEBUG" /D "_DEBUG" $(CDEFS)
!endif
!endif

all: "$(OBJDIR)" "$(OUTDIR)" "$(SHLEXT_DLL)" "$(VIEWER_EXE)"

clean: cleanobj
 -@erase "$(SHLEXT_DLL)" 2>NUL
 -@erase "$(SHLEXT_MAP)" 2>NUL
 -@erase "$(VIEWER_EXE)" 2>NUL
 -@erase "$(VIEWER_MAP)" 2>NUL
 -@rmdir "$(OBJDIR)" 2>NUL

cleanobj: cleanpdb cleanobjonly

cleanpdb:
 -@erase "$(SHLEXT_PDB)" 2>NUL
 -@erase "$(VIEWER_PDB)" 2>NUL

cleanobjonly:
 -@erase "$(OBJDIR)\*.cod" 2>NUL
 -@erase $(SHLEXT_OBJS) 2>NUL
 -@erase $(VIEWER_OBJS) 2>NUL
 -@erase "$(OBJDIR)\vc??.pdb" 2>NUL
 -@erase "$(OBJDIR)\vc??.idb" 2>NUL

"$(OBJDIR)":
 @if not exist "$(OBJDIR)/" mkdir "$(OBJDIR)"

"$(OUTDIR)":
 @if not exist "$(OUTDIR)/" mkdir "$(OUTDIR)"

"$(SHLEXT_DLL)" : $(SHLEXT_OBJS)
 $(LD) /out:$@ /map:"$(SHLEXT_MAP)" /libpath:"$(LIBDIR)" /dll /def:"dll.def" /implib:"$(OBJDIR)/thumpsvg.lib" $(LDFLAGS) $(LDLIBS) $(SHLEXT_OBJS)

"$(VIEWER_EXE)" : $(VIEWER_OBJS)
 $(LD) /out:$@ /map:"$(VIEWER_MAP)" /libpath:"$(OBJDIR)" $(LDFLAGS) $(LDLIBS) $(VIEWER_OBJS)

.SUFFIXES: .c .cpp .obj .rc .res

.c{$(OBJDIR)}.obj::
 $(CC) $(CFLAGS) $(CDEFS) $<

.cpp{$(OBJDIR)}.obj::
 $(CC) $(CFLAGS) $(CDEFS) $<

.rc{$(OBJDIR)}.res:
 $(RC) $(RFLAGS) /fo$@ $<

!include "build.dep"
thumpsvg.rc: res/reginst.inf
