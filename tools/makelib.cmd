<# :
@powershell $root='%~dp0';$argv='%*' -split '\s+';iex ((gc '%~f0') -join ';')
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

$blddir = join-path (join-path $parent 'build') $arch
ni -f -it directory $blddir | out-null

function replace-text($path, $from, $to) {
  $filepath = join-path $libdir $path
  $source = gc $filepath -raw
  $destination = $source -replace $from, $to
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

if ($build) {
  $clang = (gcm clang-cl -erroraction ignore).path
  if (!$clang) {
    $vcpath = split-path -parent ((gi 'Registry::HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\devenv.exe').getvalue('') -replace '^"|"$', '')
    if ($arch -eq 32) {
      $clang = join-path -r $vcpath '..\..\VC\Tools\Llvm\bin' -erroraction ignore
    } elseif ($arch -eq 64) {
      $clang = join-path -r $vcpath '..\..\VC\Tools\Llvm\x64\bin' -erroraction ignore
    }
    if ($clang -and (test-path (join-path $clang 'clang-cl.exe'))) {
      $env:Path += ";$clang"
    } else {
      throw "Clang not available"
    }
  }
  $cmake = (gcm cmake.exe -erroraction ignore).path
  if (!$cmake) {
    $vcpath = split-path -parent ((gi 'Registry::HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\devenv.exe').getvalue('') -replace '^"|"$', '')
    $cmake = join-path -r $vcpath 'CommonExtensions\Microsoft\CMake\CMake\bin'
    if ($cmake -and (test-path (join-path $cmake 'cmake.exe'))) {
      $env:Path += ";$cmake"
    } else {
      throw 'CMake not available'
    }
  }
}

<# zlib #>
write-host
if ($clean) {
  write-host 'Cleaning zlib...' -f yellow
  $zlibbuild = join-path $libdir 'zlib\build'
  ri -r -fo $zlibbuild -erroraction ignore
}
if ($build) {
  write-host 'Building zlib...' -f magenta
  $dir = join-path $libdir 'zlib'
  pushd $dir
  $proc = start 'cmake' -a @('-B', 'build') -nn -wait -passthru
  if ($proc.exitcode -eq 0) {
    $proc = start 'cmake' -a @('--build', 'build', '--config', 'Release') -nn -wait -passthru
  }
  popd
  if ($proc.exitcode -ne 0) {
    return
  }
}
<# zlib #>

function do-build($name, $dir = $null) {
  write-host
  if (!$dir) {
    $dir = $name
  }
  $dir = join-path $libdir $dir
  $result = $false
  pushd $dir
  if ($clean) {
    write-host "Cleaning $name..." -f yellow
    start 'cargo' -a @('clean') -nn -wait
  }
  if ($build) {
    write-host "Building $name..." -f magenta
    $proc = start 'cargo' -a @('build', '--release') -nn -wait -passthru
    if ($proc.exitcode -ne 0) {
      $result = $true
    }
  }
  popd
  return $result
}

if (do-build 'resvg') {
  return
}
if (do-build 'resvg-capi' 'resvg\c-api') {
  return
}

$bldincdir = join-path $blddir 'inc'
$bldlibdir = join-path $blddir 'lib'

write-host
if ($clean) {
  write-host 'Cleaning build files...' -f yellow
  ri -r -fo $bldincdir -erroraction ignore
  ri -r -fo $bldlibdir -erroraction ignore
}
if ($build) {
  write-host 'Copying build files...' -f cyan
  ni -f -it directory $bldincdir | out-null
  ni -f -it directory $bldlibdir | out-null
  $incdir = join-path $libdir 'resvg\c-api'
  deploy-files $incdir $bldincdir @('resvg.h')
  $reldir = join-path $libdir 'resvg\target\release'
  deploy-files $reldir $bldlibdir @('resvg.dll.lib', 'resvg.lib')
  deploy-files $reldir $blddir @('resvg.dll', 'resvg.exe', 'resvg.pdb')
  <# zlib #>
  $incdir = join-path $libdir 'zlib'
  deploy-files $incdir $bldincdir @('zlib.h')
  $incdir = join-path $libdir 'zlib\build'
  deploy-files $incdir $bldincdir @('zconf.h')
  $reldir = join-path $libdir 'zlib\build\Release'
  deploy-files $reldir $bldlibdir @('zlibstatic.lib', 'zlib.lib')
  deploy-files $reldir $blddir @('minigzip.exe', 'example.exe', 'zlib.dll')
  <# zlib #>
}

write-host 'Done!' -f green
