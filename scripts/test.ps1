param([string]$CompilerVersion, [ValidateSet("All", "Core", "Editor")][string]$Suite = "All")
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
try {
  $env:INCLUDE = "$tools\include;$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\shared;$sdkRoot\Include\$sdkVersion\um"
  $env:LIB = "$tools\lib\x64;$sdkRoot\Lib\$sdkVersion\ucrt\x64;$sdkRoot\Lib\$sdkVersion\um\x64"
  $env:PATH = "$tools\bin\Hostx64\x64;$env:PATH"
  $output = Join-Path $projectRoot 'build\tests'
  New-Item -ItemType Directory -Force $output | Out-Null
  if ($Suite -ne "Editor") {
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" "$projectRoot\tests\regression.cpp" "/Fo$output\regression.obj" "/Fe$output\regression.exe" /link /OPT:REF user32.lib shell32.lib advapi32.lib winmm.lib
    if ($LASTEXITCODE) { throw 'Regression test compilation failed.' }
    & "$output\regression.exe"
    if ($LASTEXITCODE) { throw 'Regression tests failed.' }
  }
  if ($Suite -ne "Core") {
    $editorSources = @("$projectRoot\tests\editor.cpp") + @(Get-ChildItem "$projectRoot\src" -Filter *.cpp -Recurse | Where-Object Name -notin @('main.cpp', 'macros.cpp') | Select-Object -ExpandProperty FullName)
    $editorOutput = Join-Path $output 'editor'
    New-Item -ItemType Directory -Force $editorOutput | Out-Null
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" $editorSources "/Fo$editorOutput\" "/Fe$editorOutput\editor.exe" /link /OPT:REF gdiplus.lib wininet.lib comdlg32.lib comctl32.lib uxtheme.lib shell32.lib user32.lib gdi32.lib advapi32.lib winmm.lib ole32.lib oleaut32.lib uuid.lib
    if ($LASTEXITCODE) { throw 'Editor test compilation failed.' }
    & "$editorOutput\editor.exe"
    if ($LASTEXITCODE) { throw 'Editor tests failed.' }

  }
} finally {
  $env:INCLUDE = $oldInclude; $env:LIB = $oldLib; $env:PATH = $oldPath
}
