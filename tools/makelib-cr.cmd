<# :
@powershell $self='%~f0';$root='%~dp0';$argv='%*' -split '\s+';iex ((gc '%~f0') -join ';')
@exit /b 0
#>

$parent = split-path -parent $root
$libdir = join-path $parent 'lib'

$build = $true
$clean = $false

if ($env:PROCESSOR_ARCHITECTURE -eq 'AMD64') {
  $arch = 64
} else {
  $arch = 32
}

foreach ($arg in $argv) {
  if (($arg -eq '/c') -or ($arg -eq '/clean')) {
    $clean = $true
    $build = $false
  } elseif ($arg -eq '/32') {
    $arch = 32
  } elseif ($arg -eq '/64') {
    $arch = 64
  }
}

$nmake = (gcm nmake -erroraction ignore).path
if (!$nmake) {
  $vcpath = split-path -parent ((gi 'Registry::HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\devenv.exe').getvalue('') -replace '^"|"$', '')
  $vcvars = join-path -r $vcpath "..\..\VC\Auxiliary\Build\vcvars$($arch).bat"
  $args = @('/c', ('""' + $vcvars + '" && "' + $self + '" /' + $arch + ' ' + $argv + '"'))
  start 'cmd.exe' -a $args -nn -wait
  return
}

$rustup = (gcm rustup -erroraction ignore).path
if (!$rustup) {
  throw 'Rust not available'
}

$cmake = (gcm cmake.exe -erroraction ignore).path
if (!$cmake) {
  throw 'CMake not available'
}

$gmake = (gcm make -erroraction ignore).path
if (!$gmake) {
  $gmake = join-path -r $root 'make.exe' -erroraction ignore
}
$proc = start $gmake -a @('-v') -nn -wait -rse 'nul\nul' -rso 'nul' -passthru
if ($proc.errorcode) {
  $gmake = $false
}
if (!$gmake) {
  throw 'GNU Make not available'
}

$blddir = join-path (join-path $parent 'build') $arch
ni -f -it directory $blddir | out-null

function replace-text($path, $froms, $tos, $topath = $null) {
  $froms = @($froms)
  $tos = @($tos)
  $filepath = join-path $libdir $path
  $source = gc $filepath -raw
  $destination = $source
  for ($i = 0; $i -lt $froms.length; $i++) {
    $destination = $destination -replace $froms[$i], $tos[$i]
  }
  if ($topath) {
    $filepath = join-path $libdir $topath
    if (test-path $filepath) {
      $source = gc $filepath -raw
    } else {
      $source = ''
    }
  }
  if ($source -ne $destination) {
    $destination | sc $filepath -nonewline
  }
}

function copy-if-changed($source, $destination) {
  $copy = $false
  if (!(test-path $destination)) {
    $copy = $true
  }
  if (!$copy) {
    $sourcehash = (get-filehash $source).hash
    $destinationhash = (get-filehash $destination).hash
    $copy = $sourcehash -ne $destinationhash
  }
  if ($copy) {
    copy $source $destination
  }
}

function deploy-files($sourcedir, $destinationdir, $files) {
  foreach ($file in $files) {
    $source = join-path $sourcedir $file
    $destination = join-path $destinationdir $file
    copy-if-changed $source $destination
  }
}

write-host 'Patch up some files...' -f green

ri (join-path $blddir '*.lib')
ri (join-path $blddir '*.dll')

$confac = gc (join-path $libdir 'pixman\configure.ac') -raw
$major = [regex]::match($confac, 'm4_define\(\[pixman_major\], (\d+)\)').groups[1].value
$minor = [regex]::match($confac, 'm4_define\(\[pixman_minor\], (\d+)\)').groups[1].value
$micro = [regex]::match($confac, 'm4_define\(\[pixman_micro\], (\d+)\)').groups[1].value
replace-text 'pixman\pixman\pixman-version.h.in' @('@PIXMAN_VERSION_MAJOR@', '@PIXMAN_VERSION_MINOR@', '@PIXMAN_VERSION_MICRO@') @($major, $minor, $micro) 'pixman\pixman\pixman-version.h'
replace-text 'pixman\pixman\pixman-x86.c' '(&& have_feature) (\(SSE2_BITS\))' '/* $1$2 */'
replace-text 'cairo\build\Makefile.win32.common' @("clean:", "mkdir -p \$\(CFG\)/``dirname \$<``", 'rm -f', '\$\(CFG\)/\*\.') @('clean: inform', 'if not exist "$(@D)" mkdir "$(@D)"', 'del /s/q', '$(CFG)\*.')
replace-text 'cairo\build\Makefile.win32.features' @('(CAIRO_HAS_[A-Z]+_SURFACE)=1', '(CAIRO_HAS_FT_FONT)=0', '(CAIRO_HAS_INTERPRETER)=1') @('$1=0', '$1=1', '$1=0')
replace-text 'cairo\build\Makefile.win32.features-h' 'echo +\"(.*)\" *(>>?|)' 'echo $1 $2'
replace-text 'cairo\build\Makefile.win32.inform' @('echo +\"(.*)\"', "\t?@?echo *\r?\n") @('echo $1', '')
replace-text 'cairo\src\cairoint.h' '(#define\s+cairo_public\s+__declspec\(dllexport\))' "#ifndef CAIRO_WIN32_STATIC_BUILD`n$1`n#endif"
replace-text 'cairo\src\Makefile.win32' @('%cairo-system', 'PIXMAN_LIBS\) \$\(OBJECTS', 'for ([a-z]) in (.+); *do +echo +"(.*)\$\$([a-z])";\s*done', 'echo +\"(.*)\"', "\t?@?echo *\r?\n") @('%', 'PIXMAN_LIBS) $(DEP_LIBS) $(OBJECTS', 'for %%$1 in ($2) do @echo $3%%$4', 'echo $1', '')

function do-task($name, $tcmd, $targs, $dir = $null) {
  write-host
  if ($clean) {
    write-host "Cleaning $name..." -f yellow
  }
  if ($build) {
    write-host "Building $name..." -f magenta
  }
  if (!$dir) {
    $dir = $name
  }
  $dir = join-path $libdir $dir
  pushd $dir
  $proc = start $tcmd -a $targs -nn -wait -passthru
  popd
  return $proc.exitcode -ne 0
}

$args = @('-f', 'win32\Makefile.msc')
if ($clean) {
  $args += @('clean')
}
if ($build) {
  if ($arch -eq 64) {
    $args += @('AS=ml64', 'LOC="-DASMV -DASMINF -I."', 'OBJA="inffasx64.obj gvmat64.obj inffas8664.obj"')
  } else {
    <# VC++ complains safeseh blah... #>
    <# $args += @('LOC="-DASMV -DASMINF"', 'OBJA="inffas32.obj match686.obj"') #>
  }
}
if (do-task 'zlib' 'nmake' $args) {
  return
}

$args = @('-f', 'scripts\makefile.vcwin32')
if ($clean) {
  $args += @('clean')
}
if (do-task 'libpng' 'nmake' $args) {
  return
}

$args = @('-f', 'Makefile.win32', 'CFG=release', 'MMX=off', 'SSE2=on', 'SSSE3=off')
if ($clean) {
  $args += @('clean')
}
if (do-task 'pixman' $gmake $args 'pixman\pixman') {
  return
}

write-host
if ($clean) {
  write-host 'Cleaning freetype2...' -f yellow
  $ftbuild = join-path $libdir 'freetype2\build'
  ri -r -fo $ftbuild -erroraction ignore
}
if ($build) {
  $args = @('-B', 'build', '-D', 'CMAKE_BUILD_TYPE=Release', '-D', 'CMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE', '-D', 'CMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE', '-D', 'CMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE', '-D', 'CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE', '-D', 'CMAKE_DISABLE_FIND_PACKAGE_BrotliDec=TRUE', '-D', 'SKIP_INSTALL_ALL=TRUE')
  if ($arch -eq 64) {
    $args += @('-A', 'x64')
  } else {
    $args += @('-A', 'Win32')
  }
  write-host 'Building freetype2...' -f magenta
  $dir = join-path $libdir 'freetype2'
  pushd $dir
  $proc = start $cmake -a $args -nn -wait -passthru
  if ($proc.exitcode -eq 0) {
    $proc = start $cmake -a @('--build', 'build', '--config', 'Release') -nn -wait -passthru
  }
  popd
  if ($proc.exitcode -ne 0) {
    return
  }
}

$env:INCLUDE += ';' + (join-path $libdir 'freetype2\include')
$args = @('-f', 'Makefile.win32', 'CFG=release')
if ($clean) {
  $args += @('clean')
}
if ($build) {
  $args += @('static', 'DEP_LIBS="' + (@('gdi32.lib', 'msimg32.lib', 'user32.lib', '..\..\freetype2\build\Release\freetype.lib', '..\..\zlib\zlib.lib', '..\..\libpng\libpng.lib', '..\..\pixman\pixman\release\pixman-1.lib') -join ' ') + '"')
}
if (do-task 'cairo' $gmake $args 'cairo\src') {
  return
}

if ($arch -eq 64) {
  $target = 'x86_64-pc-windows-msvc'
} else {
  $target = 'i686-pc-windows-msvc'
}
$args = @('run', ('stable-' + $target))
if ($clean) {
  ri -fo (join-path $libdir 'resvg-cairo\resvg-cairo\cairo.lib') -erroraction ignore
  $args += @('cargo', 'clean')
}
if ($build) {
  cpi -fo (join-path $libdir 'resvg-cairo\usvg\c-api\usvg_capi.rs') (join-path $libdir 'resvg-cairo\resvg-cairo\c-api\usvg_capi.rs') | out-null
  cpi -fo (join-path $libdir 'cairo\src\release\cairo-static.lib') (join-path $libdir 'resvg-cairo\resvg-cairo\cairo.lib') | out-null
  $args += @('cargo', 'build', '--release', ('--target=' + $target))
}
if (do-task 'resvg-cairo' 'rustup' $args 'resvg-cairo\resvg-cairo') {
  return
}
if (do-task 'resvg-cairo-capi' 'rustup' $args 'resvg-cairo\resvg-cairo\c-api') {
  return
}

$bldincdir = join-path $blddir 'inc\cairo'
$bldlibdir = join-path $blddir 'lib\cairo'

write-host
if ($clean) {
  write-host 'Cleaning build files...' -f yellow
  ri -r -fo $bldincdir -erroraction ignore
  ri -r -fo $bldlibdir -erroraction ignore
}
if ($build) {
  write-host 'Copying build files...' -f magenta
  ni -f -it directory $bldincdir | out-null
  ni -f -it directory $bldlibdir | out-null
  $incdir = join-path $libdir 'resvg-cairo\usvg\c-api'
  deploy-files $incdir $bldincdir @('resvg.h')
  $incdir = join-path $libdir 'resvg-cairo\resvg-cairo\c-api'
  deploy-files $incdir $bldincdir @('resvg-cairo.h')
  $incdir = join-path $libdir 'cairo\src'
  deploy-files $incdir $bldincdir @('cairo.h', 'cairo-deprecated.h', 'cairo-features.h', 'cairo-version.h', 'cairo-win32.h')
  $reldir = join-path $libdir 'libpng'
  deploy-files $reldir $bldlibdir @('libpng.lib')
  $reldir = join-path $libdir 'pixman\pixman\release'
  deploy-files $reldir $bldlibdir @('pixman-1.lib')
  $reldir = join-path $libdir 'freetype2\build\release'
  deploy-files $reldir $bldlibdir @('freetype.lib')
  $reldir = join-path $libdir 'cairo\src\release'
  deploy-files $reldir $bldlibdir @('cairo-static.lib')
  $reldir = join-path (join-path (join-path $libdir 'resvg-cairo\resvg-cairo\target') $target) 'release'
  deploy-files $reldir $bldlibdir @('resvg_cairo.dll.lib', 'resvg_cairo.lib')
  deploy-files $reldir $blddir @('resvg_cairo.dll', 'resvg-cairo.exe')
  $reldir = join-path $libdir 'zlib'
  deploy-files $reldir $bldlibdir @('zdll.lib', 'zlib.lib')
  deploy-files $reldir $blddir @('minigzip.exe', 'minigzip_d.exe', 'zlib1.dll')
}

write-host 'Done!' -f green
