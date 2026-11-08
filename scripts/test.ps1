param([string]$CompilerVersion, [ValidateSet("All", "Core", "Editor", "Main", "Capture", "Benchmark", "Storage")][string]$Suite = "All")
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
  if ($Suite -in @("All", "Storage")) {
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /DUNICODE /D_UNICODE /I "$projectRoot\src" "$projectRoot\tests\storage.cpp" "/Fo$output\storage.obj" "/Fe$output\storage.exe" /link user32.lib shell32.lib
    if ($LASTEXITCODE) { throw 'Storage test compilation failed.' }
    & "$output\storage.exe"
    if ($LASTEXITCODE) { throw 'Storage tests failed.' }
  }
  if ($Suite -in @("All", "Core")) {
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" "$projectRoot\tests\regression.cpp" "/Fo$output\regression.obj" "/Fe$output\regression.exe" /link /OPT:REF gdi32.lib user32.lib shell32.lib advapi32.lib winmm.lib
    if ($LASTEXITCODE) { throw 'Regression test compilation failed.' }
    & "$output\regression.exe"
    if ($LASTEXITCODE) { throw 'Regression tests failed.' }
  }
  if ($Suite -in @("All", "Benchmark")) {
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /O2 /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" "$projectRoot\tests\matcher_benchmark.cpp" "/Fo$output\matcher_benchmark.obj" "/Fe$output\matcher_benchmark.exe" /link /OPT:REF user32.lib gdi32.lib advapi32.lib shell32.lib winmm.lib ole32.lib
    if ($LASTEXITCODE) { throw 'Matcher benchmark compilation failed.' }
    & "$output\matcher_benchmark.exe"
    if ($LASTEXITCODE) { throw 'Matcher benchmark failed.' }
  }
  if ($Suite -in @("All", "Editor")) {
    $editorSources = @("$projectRoot\tests\editor.cpp") + @(Get-ChildItem "$projectRoot\src" -Filter *.cpp -Recurse | Where-Object Name -notin @('main.cpp', 'macros.cpp', 'config.cpp') | Select-Object -ExpandProperty FullName)
    $editorOutput = Join-Path $output 'editor'
    New-Item -ItemType Directory -Force $editorOutput | Out-Null
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" $editorSources "/Fo$editorOutput\" "/Fe$editorOutput\editor.exe" /link /OPT:REF gdiplus.lib wininet.lib comdlg32.lib comctl32.lib uxtheme.lib shell32.lib user32.lib gdi32.lib advapi32.lib winmm.lib ole32.lib oleaut32.lib uuid.lib
    if ($LASTEXITCODE) { throw 'Editor test compilation failed.' }
    & "$editorOutput\editor.exe"
    if ($LASTEXITCODE) { throw 'Editor tests failed.' }

  }
  if ($Suite -in @("All", "Capture")) {
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" "$projectRoot\tests\capture.cpp" "/Fo$output\capture.obj" "/Fe$output\capture.exe" /link /OPT:REF user32.lib gdi32.lib comctl32.lib gdiplus.lib ole32.lib shell32.lib uuid.lib
    if ($LASTEXITCODE) { throw 'Capture test compilation failed.' }
    & "$output\capture.exe"
    if ($LASTEXITCODE) { throw 'Capture tests failed.' }
  }
  if ($Suite -in @("All", "Main")) {
    $mainSources = @("$projectRoot\tests\main_ui.cpp") + @(Get-ChildItem "$projectRoot\src" -Filter *.cpp -Recurse | Where-Object Name -notin @('main.cpp', 'config.cpp') | Select-Object -ExpandProperty FullName)
    $mainOutput = Join-Path $output 'main'
    New-Item -ItemType Directory -Force $mainOutput | Out-Null
    & "$tools\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc /MT /W3 /Gy /DUNICODE /D_UNICODE /I "$projectRoot\src" $mainSources "/Fo$mainOutput\" "/Fe$mainOutput\main_ui.exe" /link /SUBSYSTEM:CONSOLE /OPT:REF gdiplus.lib wininet.lib comdlg32.lib comctl32.lib uxtheme.lib shell32.lib user32.lib gdi32.lib advapi32.lib winmm.lib ole32.lib oleaut32.lib uuid.lib
    if ($LASTEXITCODE) { throw 'Main UI test compilation failed.' }
    & "$mainOutput\main_ui.exe"
    if ($LASTEXITCODE) { throw 'Main UI tests failed.' }
  }
} finally {
  $env:INCLUDE = $oldInclude; $env:LIB = $oldLib; $env:PATH = $oldPath
}
