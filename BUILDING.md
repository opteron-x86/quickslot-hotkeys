# Building Quickslot

Clone the repository or extract a release source archive. Dependencies and their revisions are recorded in `vendor/dependencies.json`.

## Windows DLL

Install Visual Studio 2022 C++ Build Tools, a Windows SDK, LLVM with clang-cl, CMake 3.24+, Ninja, Git and Python 3.11+. Run these commands from an x64 developer PowerShell with LLVM on `PATH`:

```powershell
git clone https://github.com/opteron-x86/quickslot-hotkeys.git
cd quickslot-hotkeys
git clone https://github.com/microsoft/vcpkg.git .deps/vcpkg
git -C .deps/vcpkg checkout 00c5775211f45cd08b37fce0484b4cb940e422ab
./.deps/vcpkg/bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = "$PWD/.deps/vcpkg"
$env:PYTHONUTF8 = '1'
cmake --preset windows-release
cmake --build --preset windows-release
python tools/test-windows.py --build build/windows --output build/test-results
```

The DLL and PDB are in `build/windows`. The DLL includes SE, AE and VR support. vcpkg supplies static libraries with the dynamic Microsoft C runtime.

Install into a staging directory with:

```powershell
cmake --install build/windows --prefix build/package
```

See [Releasing](docs/RELEASING.md) for ZIP packages and GitHub Releases.

## Linux tests and UI preview

Install a C++23 compiler, CMake, Ninja, pkg-config, SDL2 and OpenGL development packages, and Python 3.11+. GitHub Actions uses GCC 14 on Ubuntu 24.04.

```sh
cmake --preset native
cmake --build --preset native
ctest --preset native
python3 -m unittest discover -s tests -p 'test_*.py' -v
./build/native/quickslot_preview .
```

## Linux cross-compilation

Install clang-cl, lld-link, LLVM resource/manifest tools and an xwin Windows SDK/CRT. Set `XWIN_SYSROOT` to the SDK and `QUICKSLOT_WINDOWS_PREFIX` to the Windows dependency prefix. Those libraries use the Microsoft x64 ABI, static linkage and dynamic CRT.

```sh
cmake -S . -B build/windows-local -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vendor/commonlib/cmake/toolchain-linux-clangcl.cmake" \
  -DXWIN_SYSROOT="$XWIN_SYSROOT" \
  -DCMAKE_PREFIX_PATH="$QUICKSLOT_WINDOWS_PREFIX"
cmake --build build/windows-local --parallel 4
python3 tools/test-windows.py --build build/windows-local --output build/test-results
```

The probe runner uses Wine on Linux. Native tests cover the editor and model; Windows probes cover DLL loading, rendering state and integration interfaces. [Gameplay checks](docs/TESTING.md) cover equipment behavior and controller/headset testing.
