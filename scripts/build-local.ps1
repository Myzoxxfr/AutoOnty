param([string]$CompilerVersion)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ tools not found.' }
$toolsRoot = Join-Path $installation 'VC\Tools\MSVC'
if (-not $CompilerVersion) {
  $CompilerVersion = (Get-ChildItem $toolsRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
}
$tools = Join-Path $toolsRoot $CompilerVersion
$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdkVersion = (Get-ChildItem "$sdkRoot\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$oldInclude = $env:INCLUDE; $oldLib = $env:LIB; $oldPath = $env:PATH
Push-Location $projectRoot
try {
  $env:INCLUDE = "$tools\include;$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\shared;$sdkRoot\Include\$sdkVersion\um"
  $env:LIB = "$tools\lib\x64;$sdkRoot\Lib\$sdkVersion\ucrt\x64;$sdkRoot\Lib\$sdkVersion\um\x64"
  $env:PATH = "$tools\bin\Hostx64\x64;$sdkRoot\bin\$sdkVersion\x64;$env:PATH"
  $output = Join-Path $projectRoot 'build\features'
  New-Item -ItemType Directory -Force $output | Out-Null
  $sources = Get-ChildItem "$projectRoot\src" -Filter *.cpp -Recurse
  foreach ($source in $sources) {
    $obj = Join-Path $output ($source.BaseName + ".obj")
    # Compile all sources: shared headers contain global configuration layout.
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /c /std:c++20 /EHsc /MT /O2 /W3 /Gy /DUNICODE /D_UNICODE /DNDEBUG /DWIN32 /D_WINDOWS /I "$projectRoot\src" $source.FullName "/Fo$output\$($source.BaseName).obj"
    if ($LASTEXITCODE) { throw "Compilation failed: $($source.Name)" }
  }
  & "$sdkRoot\bin\$sdkVersion\x64\rc.exe" /nologo "/fo$output\app.res" "$projectRoot\src\app.rc"
  if ($LASTEXITCODE) { throw 'Resources failed.' }
  $objects = Get-ChildItem $output -Filter *.obj | Where-Object Name -ne 'regression.obj' | Select-Object -ExpandProperty FullName
  & "$tools\bin\Hostx64\x64\link.exe" /nologo /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /MANIFEST:EMBED "/MANIFESTINPUT:$projectRoot\src\app.manifest" "/OUT:$output\AutoOnty.exe" $objects "$output\app.res" gdiplus.lib wininet.lib comdlg32.lib comctl32.lib uxtheme.lib shell32.lib user32.lib gdi32.lib advapi32.lib winmm.lib ole32.lib oleaut32.lib uuid.lib
  if ($LASTEXITCODE) { throw 'Link failed.' }
  Write-Output "Full application built: $output\AutoOnty.exe"
} finally {
  Pop-Location
  $env:INCLUDE = $oldInclude; $env:LIB = $oldLib; $env:PATH = $oldPath
}
