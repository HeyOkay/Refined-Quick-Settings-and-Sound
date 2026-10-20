$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$metadata=Get-Content -LiteralPath (Join-Path $root 'sound-native.wh.cpp') -Raw
$modId=[regex]::Match($metadata,'(?m)^// @id\s+(\S+)').Groups[1].Value
$modVersion=[regex]::Match($metadata,'(?m)^// @version\s+(\S+)').Groups[1].Value
if(!$modId -or !$modVersion){throw 'Missing mod ID or version in metadata'}
& python (Join-Path $root 'package.py')
if($LASTEXITCODE){throw 'Packaging failed'}
New-Item -ItemType Directory -Force (Join-Path $root 'build-temp') | Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
$env:TEMP=Join-Path $root 'build-temp';$env:TMP=$env:TEMP
Push-Location 'C:/Program Files/Windhawk/Compiler'
try{
 # Arguments go through a response file (POSIX quoting): Windows PowerShell 5.1
 # strips the inner quotes of -DWH_MOD_ID=L"..." when it starts a native program.
 $arguments=@('-std=c++23','-O1','-DUNICODE','-D_UNICODE','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00','-DWINVER=0x0A00','-DNTDDI_VERSION=0x0A000008','-target','x86_64-w64-mingw32','-shared','-DWH_MOD',('-DWH_MOD_ID=L"'+$modId+'"'),('-DWH_MOD_VERSION=L"'+$modVersion+'"'),'-include','windhawk_api.h',(Join-Path $root ('dist/'+$modId+'.wh.cpp')),'C:/Program Files/Windhawk/Engine/1.7.3/64/windhawk.lib','-lcomctl32','-lole32','-loleaut32','-lruntimeobject','-liphlpapi','-lshlwapi','-lshell32','-lversion','-luuid','-Wl,--export-all-symbols','-o',(Join-Path $root 'sound-native.dll'))
 $rsp=Join-Path $env:TEMP 'clang-args.rsp'
 [IO.File]::WriteAllLines($rsp,[string[]]($arguments|ForEach-Object{"'"+($_ -replace "'","'\''")+"'"}))
 & './bin/clang++.exe' '--rsp-quoting=posix' "@$rsp"
 if($LASTEXITCODE){throw "Compilation failed: $LASTEXITCODE"}
}finally{Pop-Location;$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
