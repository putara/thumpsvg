<# :
@powershell $root='%~dp0';$outpath='%1';iex ((gc '%~f0') -join ';')
@exit /b 0
#>

$parent = split-path -parent $root
$srcdir = join-path $parent 'src'

function find-includes($source, $dir) {
  $files = @($source)
  $done = @{}
  $result = @()
  while ($files.count) {
    $file, $files = $files
    if ($files -is [string]) {
      $files = @($files)
    } elseif ($files -eq $null) {
      $files = @()
    }
    if ($done.keys -notcontains $file) {
      $path = join-path $dir $file
      if (!(test-path $path)) {
        write-host -f red "$path does not exist, referenced by $source"
        continue
      }
      $incs = gc $path | sls '#include\s+".*"' | %{$_ -replace '#include\s+"([^"]+)"', '$1'}
      $files += $incs
      $result += $incs
      $done.$file = $true
    }
  }
  return ($result | sort -u)
}

$lines = (gci (join-path $srcdir '*') -i @('*.cpp', '*.rc')).name | %{ $incs = find-includes $_ $srcdir; if ($incs) { $_ + ': ' + ($incs -join ' ') } }
if ($outpath) {
  ($lines + '') -join "`n" | out-file -nonewline -e ascii $outpath
} else {
  $lines
}
