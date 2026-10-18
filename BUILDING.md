# Building Quickslot 0.9.0 RC5

Extract the complete source archive, including `vendor/`. CMake builds Quickslot and CommonLib from these sources. vcpkg supplies the remaining Windows libraries using the included baseline and overlay ports.

## Dependency versions

| Dependency | Version / revision |
| --- | --- |
| alandtse/CommonLibSSE-NG | 9.0.0; `5decf47b01dde5501b03afaa91cd4d182e793cca` |
| Dear ImGui | `6ded5230d043aa32c755e65c910c2af5002fb9f9` |
| ImGui VR Helper API / Client SDK | 1.9.4; `91f63900d0e6896617dd326ce1feeb914308bc42` |
| MinHook hde64 decoder | v1.3.4; `c3fcafdc10146beb5919319d0683e44e3c30d537` |
| nlohmann/json | 3.12.0 |
| fmt | 12.2.0 |
| spdlog | 1.17.0 |
| DirectXTK | 2026-05-07 |
| DirectXMath | 2026-06-12 |
| rapidcsv | 8.99 |
| vcpkg | `00c5775211f45cd08b37fce0484b4cb940e422ab` |

## Windows x64 DLL

Use Visual Studio Build Tools with a C++23-capable compiler and Windows SDK, CMake 3.24+, Ninja and vcpkg. Bootstrap vcpkg before configuring. From an x64 developer PowerShell, run these commands in the extracted source directory. Set `VCPKG_ROOT` to your vcpkg checkout.

```powershell
cmake -S . -B build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-quickslot `
  "-DVCPKG_OVERLAY_TRIPLETS=$PWD/cmake/triplets" `
  "-DVCPKG_OVERLAY_PORTS=$PWD/cmake/vcpkg-ports"
cmake --build build-release --parallel 4
cmake --install build-release --prefix package
```

The DLL is written to `package/SKSE/Plugins/Quickslot.dll`. CommonLib builds with SE, AE and VR enabled.

## Linux cross-compilation

Use clang-cl, lld-link, LLVM resource/manifest tools, and an xwin Windows SDK/CRT sysroot. Set `XWIN_SYSROOT` to that sysroot and `QUICKSLOT_WINDOWS_PREFIX` to the installed dependency prefix built for the Microsoft x64 ABI with a dynamic CRT and static libraries.

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vendor/commonlib/cmake/toolchain-linux-clangcl.cmake" \
  -DXWIN_SYSROOT="$XWIN_SYSROOT" \
  -DCMAKE_PREFIX_PATH="$QUICKSLOT_WINDOWS_PREFIX"
cmake --build build-release --parallel 4
cmake --install build-release --prefix package
```

## Portable tests and UI preview

On Linux, install a C++23 compiler, CMake, Ninja, pkg-config, SDL2 development files and OpenGL development files. The source archive supplies ImGui and the JSON headers.

```sh
cmake -S . -B build-native -G Ninja
cmake --build build-native --parallel 4
ctest --test-dir build-native --output-on-failure
./build-native/quickslot_preview .
```

