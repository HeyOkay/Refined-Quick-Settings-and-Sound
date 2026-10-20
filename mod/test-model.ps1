$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$testTemp=Join-Path $root 'build-temp'
New-Item -ItemType Directory -Force $testTemp | Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$oldPath=$env:PATH
$env:TEMP=$testTemp;$env:TMP=$testTemp
Push-Location 'C:/Program Files/Windhawk/Compiler'
try{
 & './bin/clang++.exe' '-std=c++23' '-O1' '-DUNICODE' '-D_UNICODE' '-DWIN32_LEAN_AND_MEAN' '-D_WIN32_WINNT=0x0A00' '-DWINVER=0x0A00' '-DNTDDI_VERSION=0x0A000008' '-target' 'x86_64-w64-mingw32' (Join-Path $root 'model-tests.cpp') '-lole32' '-loleaut32' '-lruntimeobject' '-lpowrprof' '-o' (Join-Path $root 'model-tests.exe')
 if($LASTEXITCODE){throw "Test compilation failed: $LASTEXITCODE"}
 # Windhawk's compiler links its runtime with the .whl suffix.
 $env:PATH='C:/ProgramData/Windhawk/Engine/Mods/64;'+$oldPath
 & (Join-Path $root 'model-tests.exe')
 if($LASTEXITCODE){throw "Property interface tests failed: $LASTEXITCODE"}
}finally{Pop-Location;$env:TEMP=$oldTemp;$env:TMP=$oldTmp;$env:PATH=$oldPath}
