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

include_directories(${CMAKE_SOURCE_DIR}/inc/)
include_directories(${CMAKE_SOURCE_DIR}/lib/resvg/capi/include)

set(SVG_DLLS "libbz2-1;libcairo-2;libdatrie-1;libexpat-1;libffi-6;libfontconfig-1;libfreetype-6;libfribidi-0;libgdk_pixbuf-2.0-0;libgio-2.0-0;libglib-2.0-0;libgmodule-2.0-0;libgobject-2.0-0;libgraphite2;libharfbuzz-0;libiconv-2;libintl-8;libpango-1.0-0;libpangocairo-1.0-0;libpangoft2-1.0-0;libpangowin32-1.0-0;libpcre-1;libpixman-1-0;libpng16-16;libstdc++-6;libthai-0;libwinpthread-1;zlib1")

if(BITS EQUAL 64)
    set(SVG_DLLS "${SVG_DLLS};libgcc_s_seh-1")
endif()

if(BITS EQUAL 32)
    set(SVG_DLLS "${SVG_DLLS};libgcc_s_dw2-1")
endif()

set(WIN32_LIBS "gdi32;shell32;shlwapi;ole32")
set(SVG_LIBS "cairo")

find_package(PkgConfig REQUIRED)
set(RESVG_LIB ${CMAKE_SOURCE_DIR}/lib/resvg.dll.lib)


project(svgtest)
add_executable(svgtest svgtest.cpp)

foreach(LIB ${WIN32_LIBS})
    target_link_libraries(svgtest ${LIB})
endforeach()

set(PROJS "thumpsvg;svgview")

add_library(thumpsvg SHARED thumpsvg.cpp dll.rc dll.def)

set(DLLRC_DEPS
    ${CMAKE_SOURCE_DIR}/dll.h
    ${CMAKE_SOURCE_DIR}/res/reginst.inf
    ${CMAKE_SOURCE_DIR}/res/manifest.xml
)
set_source_files_properties(dll.rc PROPERTIES OBJECT_DEPENDS "${DLLRC_DEPS}")

add_executable(svgview WIN32 svgview.cpp)

foreach(PROJ ${PROJS})
    project(${PROJ})
    target_link_libraries(${PROJ} ${RESVG_LIB})

    add_custom_command(TARGET ${PROJ} PRE_LINK
        COMMAND dlltool -d "${CMAKE_SOURCE_DIR}/lib/resvg.dll.def" -l "${RESVG_LIB}"
        COMMAND cargo build --release --features="cairo-backend" --manifest-path "${CMAKE_SOURCE_DIR}/lib/resvg/capi/Cargo.toml"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/lib/resvg/target/release/resvg.dll" $<TARGET_FILE_DIR:${PROJ}>)

    foreach(LIB ${SVG_LIBS})
        pkg_check_modules(LIB REQUIRED ${LIB})
        target_include_directories(${PROJ} PUBLIC ${LIB_INCLUDE_DIRS})
        target_compile_options(${PROJ} PUBLIC ${LIB_CFLAGS_OTHER})
        target_link_libraries(${PROJ} ${LIB_LIBRARIES})
    endforeach()

    foreach(LIB ${WIN32_LIBS})
        target_link_libraries(${PROJ} ${LIB})
    endforeach()

    foreach(DLL ${SVG_DLLS})
        add_custom_command(TARGET ${PROJ} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "/${MINGDIR}/bin/${DLL}.dll" $<TARGET_FILE_DIR:${PROJ}>)
    endforeach()
endforeach()

if(MINGW)
    set_target_properties(thumpsvg PROPERTIES PREFIX "")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -static-libgcc -static-libstdc++")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-stdcall-fixup -Wl,--kill-at")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--nxcompat")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--disable-auto-image-base -Wl,--dynamicbase")
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--high-entropy-va")
    endif()
endif()
