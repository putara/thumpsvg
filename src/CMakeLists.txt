cmake_minimum_required (VERSION 2.8)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BITS 64)
else(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BITS 32)
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

set(MSYSTEM_CARCH $ENV{MSYSTEM_CARCH})
if(${MSYSTEM_CARCH} MATCHES "x86_64")
    set(BITS 64)
elseif(${MSYSTEM_CARCH} MATCHES "i686")
    set(BITS 32)
endif()

set(MINGDIR "mingw${BITS}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${BITS})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${BITS})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${BITS})

include_directories(inc/)
include_directories(/${MINGDIR}/include/librsvg-2.0/librsvg/)
include_directories(/${MINGDIR}/include/glib-2.0/)
include_directories(/${MINGDIR}/include/gdk-pixbuf-2.0/)
include_directories(/${MINGDIR}/include/cairo/)
include_directories(/${MINGDIR}/include/gio-win32-2.0/)
include_directories(/${MINGDIR}/include/libxml2/)
include_directories(/${MINGDIR}/include/pango-1.0/)

set(SVG_DLLS "libbz2-1;libcairo-2;libcroco-0.6-3;libdatrie-1;libexpat-1;libffi-6;libfontconfig-1;libfreetype-6;libfribidi-0;libgdk_pixbuf-2.0-0;libgio-2.0-0;libglib-2.0-0;libgmodule-2.0-0;libgobject-2.0-0;libgraphite2;libharfbuzz-0;libiconv-2;libintl-8;liblzma-5;libpango-1.0-0;libpangocairo-1.0-0;libpangoft2-1.0-0;libpangowin32-1.0-0;libpcre-1;libpixman-1-0;libpng16-16;librsvg-2-2;libstdc++-6;libthai-0;libwinpthread-1;libxml2-2;zlib1")

if(BITS EQUAL 64)
    set(SVG_DLLS "${SVG_DLLS};libgcc_s_seh-1")
endif()

if(BITS EQUAL 32)
    set(SVG_DLLS "${SVG_DLLS};libgcc_s_dw2-1")
endif()

set(SVG_LIBS "gdi32;shell32;shlwapi;intl;cairo;rsvg-2;gio-2.0;glib-2.0;gobject-2.0")

project(thumpsvg)

add_library(thumpsvg SHARED thumpsvg.cpp dll.rc dll.def)

if(MINGW)
    set_target_properties(thumpsvg PROPERTIES PREFIX "")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -static-libgcc")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -static-libgcc")
    #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
    #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-stdcall-fixup -Wl,--kill-at")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--nxcompat")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--disable-auto-image-base -Wl,--dynamicbase")
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--high-entropy-va")
    endif()
endif()

foreach(LIB ${SVG_LIBS})
    target_link_libraries(thumpsvg ${LIB})
endforeach()

foreach(DLL ${SVG_DLLS})
    add_custom_command(TARGET thumpsvg POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "/${MINGDIR}/bin/${DLL}.dll" $<TARGET_FILE_DIR:thumpsvg>)
endforeach()

if(0)
    project(svgview)
    add_executable(svgview WIN32 svgview.c)

    foreach(LIB ${SVG_LIBS})
        target_link_libraries(svgview ${LIB})
    endforeach()

    foreach(DLL ${SVG_DLLS})
        add_custom_command(TARGET svgview POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "/${MINGDIR}/bin/${DLL}.dll" $<TARGET_FILE_DIR:svgview>)
    endforeach()
endif()
