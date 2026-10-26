param(
    [string]$RepoRoot  = $env:TIMEMARK_REPO_ROOT,
    [string]$NdkPath   = $env:ANDROID_NDK_HOME,
    [string]$ApkPath   = $env:TIMEMARK_APK_PATH,
    [string]$SampleDir = $env:TIMEMARK_SAMPLE_DIR
)

$ErrorActionPreference = "Stop"

# ============================================================================
# 0. Auto-Discover & Validate Paths (Zero Hardcoded Machine Paths)
# ============================================================================
if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $cand1 = Resolve-Path (Join-Path $PSScriptRoot "..\..") -ErrorAction SilentlyContinue
    $cand2 = Resolve-Path (Join-Path $PSScriptRoot "..\..\..") -ErrorAction SilentlyContinue
    if ($cand1 -and (Test-Path (Join-Path $cand1.Path "SO\SO")) -and (Test-Path (Join-Path $cand1.Path "Native_CPP_Reconstructed"))) {
        $RepoRoot = $cand1.Path
    } elseif ($cand2 -and (Test-Path (Join-Path $cand2.Path "SO\SO")) -and (Test-Path (Join-Path $cand2.Path "Native_CPP_Reconstructed"))) {
        $RepoRoot = $cand2.Path
    } else {
        throw "Could not auto-detect repository root from '$PSScriptRoot'. Pass -RepoRoot <path> or set TIMEMARK_REPO_ROOT."
    }
}
if (-not (Test-Path $RepoRoot)) {
    throw "Repository root does not exist: '$RepoRoot'"
}
$RepoRoot = (Resolve-Path $RepoRoot).Path

$srcRoot     = Join-Path $RepoRoot "Native_CPP_Reconstructed"
$funcDir     = Join-Path $RepoRoot "SO\Function"
$soDir       = Join-Path $RepoRoot "SO\SO"
$assetDir    = Join-Path $soDir "assets_native"
$fixturesDir = Join-Path $srcRoot "tests\fixtures"
if (-not (Test-Path $fixturesDir)) {
    $fixturesDir = Join-Path $funcDir "tests\fixtures"
}

foreach ($reqDir in @($srcRoot, $funcDir, $soDir, $assetDir, $fixturesDir)) {
    if (-not (Test-Path $reqDir)) {
        throw "Required directory missing: '$reqDir'"
    }
}

# Auto-discover Android NDK if not explicitly provided
if ([string]::IsNullOrWhiteSpace($NdkPath)) {
    foreach ($envVar in @($env:ANDROID_NDK_ROOT, $env:NDK_ROOT)) {
        if (-not [string]::IsNullOrWhiteSpace($envVar) -and (Test-Path $envVar)) {
            $NdkPath = $envVar
            break
        }
    }
}
if ([string]::IsNullOrWhiteSpace($NdkPath)) {
    $ndkSearchRoots = @()
    if ($env:LOCALAPPDATA)     { $ndkSearchRoots += Join-Path $env:LOCALAPPDATA "Android\Sdk\ndk" }
    if ($env:ANDROID_SDK_ROOT) { $ndkSearchRoots += Join-Path $env:ANDROID_SDK_ROOT "ndk" }
    if ($env:ANDROID_HOME)     { $ndkSearchRoots += Join-Path $env:ANDROID_HOME "ndk" }
    $ndkSearchRoots += @(
        "C:\Android\Sdk\ndk",
        "D:\Android\Sdk\ndk",
        "E:\AndroidSDK\ndk"
    )
    foreach ($root in $ndkSearchRoots) {
        if (Test-Path $root) {
            $dirs = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
            foreach ($d in $dirs) {
                if (Test-Path (Join-Path $d.FullName "toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe")) {
                    $NdkPath = $d.FullName
                    break
                }
            }
        }
        if (-not [string]::IsNullOrWhiteSpace($NdkPath)) { break }
    }
}
if ([string]::IsNullOrWhiteSpace($NdkPath)) {
    $unityNdks = Get-ChildItem "C:\Program Files\Unity\Hub\Editor\*\Editor\Data\PlaybackEngines\AndroidPlayer\NDK" -ErrorAction SilentlyContinue |
                 Sort-Object FullName -Descending
    foreach ($u in $unityNdks) {
        if (Test-Path (Join-Path $u.FullName "toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe")) {
            $NdkPath = $u.FullName
            break
        }
    }
}
if ([string]::IsNullOrWhiteSpace($NdkPath) -or -not (Test-Path $NdkPath)) {
    throw "Android NDK not found. Pass -NdkPath <path> or set ANDROID_NDK_HOME / ANDROID_NDK_ROOT."
}
$NdkPath  = (Resolve-Path $NdkPath).Path
$llvmRoot = Join-Path $NdkPath "toolchains\llvm\prebuilt\windows-x86_64"
$binDir   = Join-Path $llvmRoot "bin"
$clangpp  = Join-Path $binDir "clang++.exe"
$lld      = Join-Path $binDir "ld.lld.exe"
$readelf  = Join-Path $binDir "llvm-readelf.exe"
if (-not (Test-Path $readelf)) {
    $readelf = Join-Path $binDir "aarch64-linux-android-readelf.exe"
}

foreach ($tool in @($clangpp, $lld, $readelf)) {
    if (-not (Test-Path $tool)) {
        throw "Required NDK tool missing: '$tool' (NDK: '$NdkPath')"
    }
}

# Resolve SampleDir & ApkPath with bundled fixture fallbacks
if ([string]::IsNullOrWhiteSpace($SampleDir)) {
    $SampleDir = $fixturesDir
}
if (-not (Test-Path $SampleDir)) {
    throw "Sample JPEG directory does not exist: '$SampleDir'"
}
$SampleDir = (Resolve-Path $SampleDir).Path

if ([string]::IsNullOrWhiteSpace($ApkPath)) {
    $fixtureApk = Join-Path $fixturesDir "timemark_release_sig.apk"
    $rootApk    = Join-Path $RepoRoot "Timemark.apk"
    if (Test-Path $fixtureApk) {
        $ApkPath = $fixtureApk
    } elseif (Test-Path $rootApk) {
        $ApkPath = $rootApk
    } else {
        throw "Release APK fixture not found at '$fixtureApk' or '$rootApk'. Pass -ApkPath <path>."
    }
}
if (-not (Test-Path $ApkPath)) {
    throw "APK file does not exist: '$ApkPath'"
}
$ApkPath = (Resolve-Path $ApkPath).Path

$forgedApkPath = Join-Path $fixturesDir "timemark_forged_sig.apk"
$dictFixture   = Join-Path $fixturesDir "paddle_keys_ext.txt"
foreach ($reqFile in @($forgedApkPath, $dictFixture)) {
    if (-not (Test-Path $reqFile)) {
        throw "Required test fixture file missing: '$reqFile'"
    }
}

Write-Host "============================================================================"
Write-Host "  AUDIT VERIFICATION SUITE (Reports 11 & 13 - Portable Native C++ Harness)"
Write-Host "  RepoRoot  : $RepoRoot"
Write-Host "  NdkPath   : $NdkPath"
Write-Host "  ApkPath   : $ApkPath"
Write-Host "  SampleDir : $SampleDir"
Write-Host "============================================================================"

# ----------------------------------------------------------------------------
# 1. Verify Byte-for-Byte Source Synchronization
# ----------------------------------------------------------------------------
Write-Host "`n[1/8] Checking byte-for-byte synchronization across Native_CPP_Reconstructed and SO/Function..."
$srcFiles = @(
    "src\edge_ai_ncnn_jni.cpp",
    "src\xhey_media_and_security_jni.cpp",
    "src\code_detect_anticode_jni.cpp",
    "src\jpeg_packer_jni.cpp",
    "src\aegis_anti_spoof_jni.cpp",
    "src\mcap_robotics_jni.cpp",
    "include\xhey_jni_models.h"
)

foreach ($rel in $srcFiles) {
    $leaf = Split-Path $rel -Leaf
    $h1 = (Get-FileHash (Join-Path $srcRoot $rel) -Algorithm SHA256).Hash
    $h2 = (Get-FileHash (Join-Path $funcDir $rel) -Algorithm SHA256).Hash
    $h3 = (Get-FileHash (Join-Path $funcDir $leaf) -Algorithm SHA256).Hash
    if ($h1 -ne $h2 -or $h1 -ne $h3) {
        throw "Hash mismatch for $rel : $h1 vs $h2 vs $h3"
    }
    Write-Host ("  - {0,-38} : SHA256 = {1}... (3 copies identical)" -f $rel, $h1.Substring(0, 16))
}
foreach ($pairRel in @("CMakeLists.txt", "tests\verify_audit_2026_10_25.cpp", "tests\verify_audit_2026_10_25.ps1")) {
    $p1 = (Get-FileHash (Join-Path $srcRoot $pairRel) -Algorithm SHA256).Hash
    $p2 = (Get-FileHash (Join-Path $funcDir $pairRel) -Algorithm SHA256).Hash
    if ($p1 -ne $p2) {
        throw "Hash mismatch between Native_CPP_Reconstructed and SO/Function for $pairRel : $p1 vs $p2"
    }
    Write-Host ("  - {0,-38} : SHA256 = {1}... (2 copies identical)" -f $pairRel, $p1.Substring(0, 16))
}
Write-Host "  [PASS] All C++/header/CMake/test files are 100% byte-for-byte identical across directories."

# ----------------------------------------------------------------------------
# 2. Verify All 7 Built .so Libraries (PT_DYNAMIC, .dynsym/sh_info, 0 Stubs, 198 STT_FUNC, 159 Java_*)
# ----------------------------------------------------------------------------
Write-Host "`n[2/8] Auditing 7 compiled ARM64 .so libraries in SO/SO (PT_DYNAMIC, .dynsym/sh_info, STT_FUNC, Java_*)..."
$expectedSoNames = @(
    "libffmpeg.so",
    "libjpeg.so",
    "libmp3lame.so",
    "libnative-lib.so",
    "libnnpackage-f99779bd.so",
    "libxhey_videoedit.so",
    "libxheymediav8.so"
)
foreach ($soName in $expectedSoNames) {
    if (-not (Test-Path (Join-Path $soDir $soName))) {
        throw "Missing compiled shared library: $(Join-Path $soDir $soName)"
    }
}

$totalFunc = 0
$totalJava = 0
$keySymbols = @(
    "Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init",
    "Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_init",
    "Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit",
    "Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init",
    "Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startDecode",
    "Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform3",
    "Java_com_xhey_xcamera_security_SecurityManager_nativeEncrypt",
    "Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt",
    "Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature"
)
$foundKeySyms = @{}

Get-ChildItem "$soDir\*.so" | Sort-Object Name | ForEach-Object {
    $dynOut = & $readelf -d $_.FullName 2>&1
    if (-not ($dynOut | Select-String "Dynamic section at offset")) {
        throw "Missing PT_DYNAMIC in $($_.Name)"
    }

    $fileBytes = [System.IO.File]::ReadAllBytes($_.FullName)
    $stubPattern = "FD-7B-BF-A9-FD-03-00-91-00-00-80-D2-FD-7B-C1-A8-C0-03-5F-D6-1F-20-03-D5-1F-20-03-D5-1F-20-03-D5"
    $fileHex = [System.BitConverter]::ToString($fileBytes)
    if ($fileHex.Contains($stubPattern)) {
        throw "32-byte AArch64 stub pattern found in $($_.Name)"
    }

    # Validate ELF64 .dynsym sh_info local/global symbol partition, .gnu.version, and .hash
    $e_shoff     = [System.BitConverter]::ToUInt64($fileBytes, 40)
    $e_shentsize = [System.BitConverter]::ToUInt16($fileBytes, 58)
    $e_shnum     = [System.BitConverter]::ToUInt16($fileBytes, 60)
    $dynsymShInfo  = 0
    $dynsymOff     = 0
    $dynsymSize    = 0
    $dynsymEntSize = 24
    $versymOff     = 0
    $hashOff       = 0

    for ($secIdx = 0; $secIdx -lt $e_shnum; $secIdx++) {
        $secOff    = [int]$e_shoff + ($secIdx * $e_shentsize)
        $sh_type   = [System.BitConverter]::ToUInt32($fileBytes, $secOff + 4)
        $sh_offset = [int][System.BitConverter]::ToUInt64($fileBytes, $secOff + 24)
        $sh_size   = [int][System.BitConverter]::ToUInt64($fileBytes, $secOff + 32)
        $sh_entsz  = [int][System.BitConverter]::ToUInt64($fileBytes, $secOff + 56)
        if ($sh_type -eq 11) { # SHT_DYNSYM
            $dynsymShInfo = [int][System.BitConverter]::ToUInt32($fileBytes, $secOff + 44)
            $dynsymOff    = $sh_offset
            $dynsymSize   = $sh_size
            if ($sh_entsz -gt 0) { $dynsymEntSize = $sh_entsz }
        } elseif ($sh_type -eq 0x6fffffff) { # SHT_GNU_versym
            $versymOff = $sh_offset
        } elseif ($sh_type -eq 5) { # SHT_HASH
            $hashOff = $sh_offset
        }
    }

    if ($dynsymOff -le 0 -or $dynsymSize -le 0) {
        throw "Missing SHT_DYNSYM section in $($_.Name)"
    }
    $numDynsyms = [int]($dynsymSize / $dynsymEntSize)
    if ($dynsymShInfo -le 0 -or $dynsymShInfo -gt $numDynsyms) {
        throw "Invalid .dynsym sh_info=$dynsymShInfo (numDynsyms=$numDynsyms) in $($_.Name)"
    }
    for ($s = 0; $s -lt $numDynsyms; $s++) {
        $symOff   = $dynsymOff + ($s * $dynsymEntSize)
        $st_info  = $fileBytes[$symOff + 4]
        $st_shndx = [System.BitConverter]::ToUInt16($fileBytes, $symOff + 6)
        $bind     = ($st_info -shr 4) -band 0x0F
        if ($s -lt $dynsymShInfo -and $bind -ne 0) {
            throw "Non-local symbol (bind=$bind) at .dynsym index $s (< sh_info=$dynsymShInfo) in $($_.Name)"
        }
        if ($s -ge $dynsymShInfo -and $bind -eq 0) {
            throw "Invalid STB_LOCAL symbol at .dynsym index $s (>= sh_info=$dynsymShInfo) in global part of symbol table in $($_.Name)"
        }
        if ($versymOff -gt 0) {
            $ver = [System.BitConverter]::ToUInt16($fileBytes, $versymOff + ($s * 2))
            if ($s -lt $dynsymShInfo -and $ver -ne 0) {
                throw "Local symbol at .dynsym index $s has non-local .gnu.version=$ver in $($_.Name)"
            }
            if ($s -ge $dynsymShInfo -and $st_shndx -ne 0 -and $ver -eq 0) {
                throw "Defined global symbol at .dynsym index $s has local .gnu.version=0 in $($_.Name)"
            }
        }
    }
    if ($hashOff -gt 0) {
        $nbucket = [System.BitConverter]::ToUInt32($fileBytes, $hashOff)
        $nchain  = [System.BitConverter]::ToUInt32($fileBytes, $hashOff + 4)
        if ($nchain -ne $numDynsyms) {
            throw ".hash nchain ($nchain) != .dynsym count ($numDynsyms) in $($_.Name)"
        }
        for ($b = 0; $b -lt $nbucket; $b++) {
            $cur = [int][System.BitConverter]::ToUInt32($fileBytes, $hashOff + 8 + ($b * 4))
            while ($cur -ne 0) {
                if ($cur -lt $dynsymShInfo) {
                    throw "Local symbol index $cur (< sh_info=$dynsymShInfo) present in .hash chain in $($_.Name)"
                }
                $cur = [int][System.BitConverter]::ToUInt32($fileBytes, $hashOff + 8 + ($nbucket * 4) + ($cur * 4))
            }
        }
    }

    # Cross-check readelf --dyn-syms output against sh_info
    $dynSymLines = & $readelf --dyn-syms -W $_.FullName 2>&1
    foreach ($dLine in $dynSymLines) {
        $strLine = $dLine.ToString()
        if ($strLine -match "^\s*(\d+):\s+[0-9a-fA-F]+\s+\d+\s+\S+\s+(LOCAL|GLOBAL|WEAK)\s+") {
            $idx  = [int]$Matches[1]
            $bindStr = $Matches[2]
            if ($idx -lt $dynsymShInfo -and $bindStr -ne "LOCAL") {
                throw "readelf --dyn-syms: non-LOCAL symbol at index $idx (< sh_info=$dynsymShInfo) in $($_.Name)"
            }
            if ($idx -ge $dynsymShInfo -and $bindStr -eq "LOCAL") {
                throw "readelf --dyn-syms: invalid LOCAL symbol at index $idx (>= sh_info=$dynsymShInfo) in $($_.Name): $strLine"
            }
        }
    }

    $symOut = & $readelf -sW $_.FullName 2>&1
    $dynSyms = $symOut | Select-String "FUNC\s+GLOBAL\s+DEFAULT\s+\d+\s+"
    $javaSyms = $dynSyms | Select-String "Java_"
    $totalFunc += $dynSyms.Count
    $totalJava += $javaSyms.Count

    foreach ($line in $dynSyms) {
        if ($line.Line -match "^\s*\d+:\s+[0-9a-fA-F]+\s+(\d+)\s+FUNC\s+GLOBAL\s+DEFAULT\s+\d+\s+(\S+)") {
            $sz = [int]$Matches[1]
            $symName = $Matches[2] -replace "@.*$",""
            if ($keySymbols -contains $symName) {
                $foundKeySyms[$symName] = "$($_.Name): $sz bytes"
            }
        }
    }
    Write-Host ("  - {0,-26} | {1,7} B | PT_DYNAMIC=YES | sh_info={2} (LOCAL=0..{3}, GLOBAL={2}..{4,3}) | STT_FUNC={5,3} | Java_*={6,3}" -f $_.Name, $_.Length, $dynsymShInfo, ($dynsymShInfo - 1), ($numDynsyms - 1), $dynSyms.Count, $javaSyms.Count)
}
if ($totalFunc -ne 198 -or $totalJava -ne 159) {
    throw "Expected 198 STT_FUNC and 159 Java_* symbols, got $totalFunc and $totalJava"
}
Write-Host "  [PASS] All 7 .so have valid .dynsym/sh_info boundaries (0 local-in-global symbols), Total STT_FUNC = $totalFunc, Total Java_* = $totalJava, Stub count = 0/198."
foreach ($k in $keySymbols) {
    if (-not $foundKeySyms.ContainsKey($k)) {
        throw "Audited symbol missing from .so exports: $k"
    }
    Write-Host "    * $k -> $($foundKeySyms[$k])"
}

# ----------------------------------------------------------------------------
# 3. Compile, Link & Execute Native C++ Verification Harness ([LINK PASS] + [RUNTIME PASS])
# ----------------------------------------------------------------------------
Write-Host "`n[3/8] Compiling, linking, and executing native C++ verification harness (verify_audit_2026_10_25.cpp)..."

# 3A. ARM64 Link Verification against all 7 SO/SO/*.so with ld.lld --fatal-warnings ([LINK PASS])
$arm64Out = Join-Path $env:TEMP "verify_audit_2026_10_25_arm64"
Remove-Item $arm64Out -Force -ErrorAction SilentlyContinue
& $clangpp --target=aarch64-linux-android28 -fuse-ld=lld "-Wl,--fatal-warnings" -fno-addrsig -stdlib=libc++ -std=c++17 -O2 `
    "-I$srcRoot\include" `
    "$srcRoot\tests\verify_audit_2026_10_25.cpp" `
    "-L$soDir" -lnnpackage-f99779bd -lnative-lib -lxheymediav8 -lxhey_videoedit -lffmpeg -ljpeg -lmp3lame `
    -llog -landroid -ljnigraphics -lGLESv2 -lEGL -lmediandk -lm `
    -o $arm64Out
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $arm64Out)) {
    throw "[LINK FAIL] Failed to link ARM64 test harness against SO/SO/*.so"
}
$arm64Size = (Get-Item $arm64Out).Length
Remove-Item $arm64Out -Force -ErrorAction SilentlyContinue
Write-Host "  [LINK PASS] Linked ARM64 binary ($arm64Size bytes) cleanly via ld.lld (--fatal-warnings) against all 7 SO/SO/*.so libraries."

# 3B. Host Native C++ Runtime Execution via NDK clang++ + ld.lld ([RUNTIME PASS])
$buildDir = Join-Path $env:TEMP "xhey_native_runtime_verify"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Remove-Item "$buildDir\*" -Force -ErrorAction SilentlyContinue

$sysroot    = Join-Path $llvmRoot "sysroot"
$cxxInc     = Join-Path $sysroot "usr\include\c++\v1"
$cxxSiteInc = Join-Path $sysroot "usr\include\x86_64-linux-android\c++\v1"
$sysInc     = Join-Path $sysroot "usr\include"
$archInc    = Join-Path $sysroot "usr\include\x86_64-linux-android"
$clangIncCandidates = @(
    Get-ChildItem "$llvmRoot\lib64\clang\*\include" -ErrorAction SilentlyContinue
    Get-ChildItem "$llvmRoot\lib\clang\*\include" -ErrorAction SilentlyContinue
)
if ($clangIncCandidates.Count -eq 0) {
    throw "Could not locate clang builtin include directory under '$llvmRoot'"
}
$clangInc = $clangIncCandidates[0].FullName

$isystemFlags = @()
if (Test-Path $cxxSiteInc) { $isystemFlags += @("-isystem", $cxxSiteInc) }
$isystemFlags += @(
    "-isystem", $cxxInc,
    "-isystem", $clangInc,
    "-isystem", $archInc,
    "-isystem", $sysInc
)

$runtimeSources = @(
    (Join-Path $srcRoot "src\edge_ai_ncnn_jni.cpp"),
    (Join-Path $srcRoot "src\xhey_media_and_security_jni.cpp"),
    (Join-Path $srcRoot "src\code_detect_anticode_jni.cpp"),
    (Join-Path $srcRoot "src\jpeg_packer_jni.cpp"),
    (Join-Path $srcRoot "tests\verify_audit_2026_10_25.cpp")
)

foreach ($cpp in $runtimeSources) {
    $objOut = Join-Path $buildDir ((Split-Path $cpp -Leaf) + ".obj")
    & $clangpp --target=x86_64-w64-mingw32 -std=c++17 -O2 -fno-builtin -fno-exceptions -fno-rtti -mno-stack-arg-probe `
        -nostdinc -U_WIN32 -U_WIN64 -U__MINGW32__ -U__MINGW64__ -U__MSVCRT__ `
        -D__ELF__=1 -D_LIBCPP_OBJECT_FORMAT_COFF=1 -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS=1 `
        -D__linux__=1 -D__ANDROID__=1 -D__ANDROID_API__=28 -D__x86_64__=1 `
        -DXHEY_STANDALONE_TEST_SHIM=1 -DXHEY_WIN64_NDK_HOST_TEST=1 `
        $isystemFlags `
        -I (Join-Path $srcRoot "include") `
        -c $cpp -o $objOut
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $objOut)) {
        throw "Failed to compile native runtime object for: $cpp"
    }
}

$defContent = @"
LIBRARY msvcrt.dll
EXPORTS
  __getmainargs
  _vsnprintf
  abort
  abs
  acos
  asin
  atan
  atan2
  atexit
  atof
  atoi
  atol
  ceil
  clock
  cos
  exit
  exp
  fabs
  fclose
  fflush
  floor
  fopen
  fprintf
  fputc
  fputs
  fread
  free
  fseek
  ftell
  fwrite
  getenv
  labs
  log
  malloc
  memchr
  memcmp
  memcpy
  memmove
  memset
  pow
  printf
  putc
  putchar
  puts
  qsort
  rand
  realloc
  remove
  sin
  sprintf
  sqrt
  srand
  sscanf
  strchr
  strcmp
  strcpy
  strcspn
  strerror
  strlen
  strncmp
  strncpy
  strrchr
  strstr
  strtod
  strtol
  strtoul
  tan
  tanh
  tolower
  toupper
  vfprintf
  vprintf
"@
$defPath = Join-Path $buildDir "msvcrt_ndk.def"
$libPath = Join-Path $buildDir "msvcrt_ndk.lib"
$exePath = Join-Path $buildDir "verify_audit_2026_10_25_win64.exe"
$logPath = Join-Path $buildDir "verify_audit_runtime.log"

Set-Content -Path $defPath -Value $defContent -Encoding ASCII
& $lld -flavor link "/def:$defPath" "/out:$libPath" /machine:x64
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $libPath)) {
    throw "Failed to generate import library msvcrt_ndk.lib via ld.lld"
}

$objFiles = (Get-ChildItem "$buildDir\*.obj").FullName
& $lld -flavor link /subsystem:console /entry:Win64NdkEntry "/out:$exePath" $objFiles $libPath
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exePath)) {
    throw "Failed to link native runtime harness executable: $exePath"
}

$nativeOutput = & $exePath $assetDir $SampleDir 2>&1
$nativeExitCode = $LASTEXITCODE
$nativeText = ($nativeOutput | Out-String)
Set-Content -Path $logPath -Value $nativeText -Encoding UTF8

Write-Host $nativeText.TrimEnd()
if ($nativeExitCode -ne 0 -or $nativeText -notmatch "\[RUNTIME PASS\]") {
    throw "[RUNTIME FAIL] Native C++ runtime harness exited with code $nativeExitCode. See log: $logPath"
}
Write-Host "  [RUNTIME PASS] Native C++ harness main() executed (exit code = $nativeExitCode, log = $logPath)."

# ----------------------------------------------------------------------------
# 4. Verify P1 #1 & P1 #2: Real NCNN .param & .bin Assets + Native Graph Execution
# ----------------------------------------------------------------------------
Write-Host "`n[4/8] Verifying P1 #1 & P1 #2: Real NCNN .param (2-line header) & .bin weights + Fail-Closed Init..."
$models = @(
    @{ Name = "zero_dce_full_6head_more_self_data_6epoch_v2"; Layers = 22; Blobs = 30; BinSize = 303844 },
    @{ Name = "zero_dce++_full_no_resize_pre_train";          Layers = 28; Blobs = 31; BinSize = 42300  },
    @{ Name = "silero_vad_16k.ncnn";                          Layers = 67; Blobs = 82; BinSize = 621680 }
)
foreach ($m in $models) {
    $pFile = Join-Path $assetDir "$($m.Name).param"
    $bFile = Join-Path $assetDir "$($m.Name).bin"
    if (-not (Test-Path $pFile) -or -not (Test-Path $bFile)) {
        throw "Missing model asset files for $($m.Name) in '$assetDir'"
    }
    $lines = Get-Content $pFile
    $magic = [int]($lines[0].Trim())
    $counts = ($lines[1].Trim() -split "\s+")
    $lCount = [int]$counts[0]
    $bCount = [int]$counts[1]
    $bActual = (Get-Item $bFile).Length
    if ($magic -ne 7767517 -or $lCount -ne $m.Layers -or $bCount -ne $m.Blobs -or $bActual -ne $m.BinSize) {
        throw "Model header/size mismatch for $($m.Name)"
    }
    Write-Host ("  - {0,-44} | magic={1} | layers={2,2} | blobs={3,2} | bin={4,7} B" -f $m.Name, $magic, $lCount, $bCount, $bActual)
}
if ($nativeText -notmatch "\[NATIVE 1/6\][\s\S]*?\[PASS\]" -or $nativeText -notmatch "\[NATIVE 2/6\][\s\S]*?\[PASS\]") {
    throw "Native C++ NCNN execution or fail-closed checks ([NATIVE 1/6], [NATIVE 2/6]) did not pass"
}
Write-Host "  [PASS] All 3 NCNN models executed in native C++ with quantitative output & fail-closed verification."

# ----------------------------------------------------------------------------
# 5. Verify P1 #3: Native SecurityManager_nativeEncrypt / nativeDecrypt (XAE1 AEAD)
# ----------------------------------------------------------------------------
Write-Host "`n[5/8] Verifying P1 #3: Native SecurityManager_nativeEncrypt / nativeDecrypt (XAE1 AEAD)..."
if ($nativeText -notmatch "\[NATIVE 3/6\][\s\S]*?\[PASS\]") {
    throw "Native C++ XAE1 AEAD verification ([NATIVE 3/6]) did not pass"
}
Write-Host "  [PASS] Native C++ SecurityManager_nativeEncrypt/nativeDecrypt verified (unique 96-bit nonce, round-trip, fixed test vector, 3-region tamper rejection)."

# ----------------------------------------------------------------------------
# 6. Verify P1 #4: Native APK Signing Block v2 Certificate Pinning & Forgery Rejection
# ----------------------------------------------------------------------------
Write-Host "`n[6/8] Verifying P1 #4: Native APK Signing Block v2 Certificate Pinning & Forgery Rejection..."
$apkBytes = [System.IO.File]::ReadAllBytes($ApkPath)
if ($apkBytes.Length -lt 64) {
    throw "APK file too small: $ApkPath"
}
if ($nativeText -notmatch "\[NATIVE 4/6\][\s\S]*?Release APK Cert SHA-256: 96fcc4571a3c269d0bae4ca6fa786d236d8b61a691d58406c3805996f53f123f -> ACCEPTED" -or
    $nativeText -notmatch "\[NATIVE 4/6\][\s\S]*?Forged APK Cert\s*: untrusted_certificate_digest:[\s\S]*?-> REJECTED") {
    throw "Native C++ APK certificate pinning & forgery rejection ([NATIVE 4/6]) did not pass"
}
Write-Host "  [PASS] Native C++ SecurityManager_nativeVerifyAppSignature & JNIUtils_getApkSignature accepted release APK ($($apkBytes.Length) B) and rejected forged APK & untrusted package."

# ----------------------------------------------------------------------------
# 7. Verify P2 #5: Native PaddleOCR_init External Dictionary Loader (keysPath)
# ----------------------------------------------------------------------------
Write-Host "`n[7/8] Verifying P2 #5: Native PaddleOCR_init external UTF-8 dictionary loader..."
$dictTokens = (Get-Content $dictFixture -Encoding UTF8 | Where-Object { $_.Trim().Length -gt 0 })
if ($dictTokens.Count -lt 30) {
    throw "External dictionary fixture '$dictFixture' has too few tokens: $($dictTokens.Count)"
}
if ($nativeText -notmatch "\[NATIVE 5/6\][\s\S]*?\[PASS\]") {
    throw "Native C++ PaddleOCR_init external dictionary verification ([NATIVE 5/6]) did not pass"
}
Write-Host "  [PASS] Native C++ PaddleOCR_init verified with $($dictTokens.Count)-token external UTF-8 dictionary ($dictFixture)."

# ----------------------------------------------------------------------------
# 8. Verify P2 #6: Release Sample JPEGs (Strict Count == 3, SHA-256 & Native Decode)
# ----------------------------------------------------------------------------
Write-Host "`n[8/8] Verifying P2 #6: Native JpegPacker & CodeDetect on 3 SHA-256-verified release sample JPEGs..."
$expectedSamples = @(
    @{
        FileName = "2025-04-21 11.00.25_10 Bridge Street_(Company-Timemark)_(Name-Elvis).jpg"
        Size     = 2009314
        Sha256   = "85F0BB1FE51B68A278BCF623104A0C9C5785AFCD21E22EC5E92E5968EB3D5EBB"
        AntiCode = "BHPUTG9Y4K1DEW"
    },
    @{
        FileName = "2025-04-25 12.18.34_Gg Pandega Padma No. 6B_(Perusahaan-Timemark)_(Nama-Elvis).jpg"
        Size     = 1619943
        Sha256   = "F83998D65E6E88E06D0B0295526728559A5F867B20557DB071C0324AA27FFDC8"
        AntiCode = "MGT4DA9L9MBR9U"
    },
    @{
        FileName = "2025-04-28 08.59.13_Calle 61 11632_(Empresa-Timemark)_(Nombre-Elvis).jpg"
        Size     = 2195949
        Sha256   = "DA11C91C9F9B15830CCD55EB4B906E2F440B22FAF21F3294B890CA28DDB95509"
        AntiCode = "EG6P1H9HLAMGPU"
    }
)

$foundSampleFiles = @(Get-ChildItem -Path $SampleDir -Filter "2025-04-*.jpg" -File -ErrorAction SilentlyContinue | Sort-Object Name)
if ($foundSampleFiles.Count -ne 3) {
    throw "Expected exactly 3 sample JPEG files (2025-04-*.jpg) in '$SampleDir', found $($foundSampleFiles.Count)"
}

foreach ($spec in $expectedSamples) {
    $fullJp = Join-Path $SampleDir $spec.FileName
    if (-not (Test-Path -LiteralPath $fullJp)) {
        throw "Required sample JPEG fixture missing: '$fullJp'"
    }
    $actualSize = (Get-Item -LiteralPath $fullJp).Length
    $actualHash = (Get-FileHash -LiteralPath $fullJp -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($actualSize -ne $spec.Size -or $actualHash -ne $spec.Sha256) {
        throw "Fixture integrity mismatch for $($spec.FileName): size=$actualSize (expected $($spec.Size)), sha256=$actualHash (expected $($spec.Sha256))"
    }
    Write-Host ("  - {0} | {1,9:N0} B | SHA256={2}... | AntiCode={3}" -f $spec.FileName, $actualSize, $actualHash.Substring(0, 16), $spec.AntiCode)
}

if ($nativeText -notmatch "\[NATIVE 6/6\][\s\S]*?\[PASS\]") {
    throw "Native C++ JpegPacker & CodeDetect verification ([NATIVE 6/6]) did not pass"
}
Write-Host "  [PASS] All 3 sample JPEGs (strict count=3, SHA-256 verified) decoded by native C++ JpegPacker & CodeDetect."

Write-Host "`n============================================================================"
Write-Host "  ALL 8 VERIFICATION CHECKS PASSED ([LINK PASS] & [RUNTIME PASS] VERIFIED)"
Write-Host "============================================================================"
