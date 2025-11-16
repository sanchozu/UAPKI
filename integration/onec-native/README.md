# Native 1C component on top of UAPKI

This directory contains the source code of a cross-platform NativeAPI component for **1C:Enterprise 8** that wraps the UAPKI JSON API and exposes high-level methods for signing and verifying data with **DSTU 4145-2002** (and other algorithms supported by UAPKI).

## Features

* Works both on Windows and Linux thick clients of 1C (builds shared library `uapki1c.dll`/`libuapki1c.so`).
* Uses the official UAPKI JSON API (`process()`/`json_free()`) and keeps all crypto logic inside the proven library.
* Statically links the UAPKI core into the NativeAPI component, so only a single DLL/SO needs to be shipped per platform.
* Methods exported to 1C:
  * `Initialize(ПараметрыJSON)` – forwards the provided configuration directly to UAPKI `INIT`.
  * `SignFile(ПутьКФайлу, ПутьКПодписи, ПараметрыJSON)` – reads a file, signs it (default CAdES-BES, DSTU 4145-2002), stores detached signature and returns JSON summary with signer/certificate info.
  * `VerifyFileSignature(ПутьКФайлу, ПутьКПодписи, ПараметрыJSON)` – verifies detached/attached signature, returns JSON report (`signatureInfos`, certificate chain, timestamps).
  * `GetCertificateInfo(Источник)` – accepts either a path to certificate file or JSON object and returns serial, subject, issuer, validity, signature algorithm.
  * `SignData(Данные, ПараметрыJSON)` – signs an in-memory string/byte array and returns Base64 of the signature.
  * `VerifyDataSignature(Данные, ПодписьBase64, ПараметрыJSON)` – verifies Base64 signature over in-memory data, returns JSON report.
* Property `LastError` stores the textual description of the last exception so that `Попытка … Исключение` blocks in 1C can show human-readable diagnostics.

## Build instructions

1. Configure and build the component. By default the CMake project adds `../../library/` as a sub-project, builds static versions of `uapkic`, `uapkif` and `uapki`, and links them into a single NativeAPI library:

```bash
cd integration/onec-native
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

   The resulting DLL/SO already contains the UAPKI core, so there is only one binary to copy to the target machine.
2. If you still prefer to link against pre-built UAPKI artifacts, disable the embedded build (`-DUAPKI_ONEC_EMBED=OFF`) and point `UAPKI_ROOT` to the folder with `uapki-export.h` and `libuapki*`.
3. Copy the built `uapki1c.dll`/`libuapki1c.so` to a folder that is accessible for the 1C platform (place it next to the 1C executable on Windows or into a directory from `LD_LIBRARY_PATH` on Linux).

### Platform-specific notes

The project now ships a single `CMakeLists.txt` that is capable of producing binaries for all platforms supported by the 1C Native API. Use the snippets below as a starting point:

#### Microsoft Windows (x86 and x86-64)

```powershell
cmake -S integration/onec-native -B build-win64 -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-win64 --config Release

cmake -S integration/onec-native -B build-win32 -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build-win32 --config Release
```

#### Linux (x86-64, armv7, armv8)

Native x86-64 build:

```bash
cmake -S integration/onec-native -B build-linux-x86_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux-x86_64 -j
```

Cross-compiling for ARM typically requires a sysroot/toolchain. Example for armv7 (hf) and armv8:

```bash
cmake -S integration/onec-native -B build-linux-armv7 \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=armv7 \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
  -DCMAKE_SYSROOT=/opt/armv7-sysroot
cmake --build build-linux-armv7 -j

cmake -S integration/onec-native -B build-linux-arm64 \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
  -DCMAKE_SYSROOT=/opt/arm64-sysroot
cmake --build build-linux-arm64 -j
```

#### Apple macOS (x86-64 and Apple Silicon)

```bash
cmake -S integration/onec-native -B build-macos-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build-macos-x64 -j

cmake -S integration/onec-native -B build-macos-arm64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos-arm64 -j
```

#### Apple iOS / iPadOS (arm64)

```bash
cmake -S integration/onec-native -B build-ios-arm64 \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build-ios-arm64 -j
```

#### Google Android (armv8 and x86-64)

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk
cmake -S integration/onec-native -B build-android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=24
cmake --build build-android-arm64 -j

cmake -S integration/onec-native -B build-android-x86_64 \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=x86_64 \
  -DANDROID_PLATFORM=24
cmake --build build-android-x86_64 -j
```

## Registering the component in 1C

Use the standard NativeAPI mechanism:

```bsl
ПодключитьВнешнююКомпоненту("uapki", "UapkiNative", ТипВнешнейКомпоненты.Native);
Компонента = Новый ("AddIn.UapkiNative");
```

Available methods (names are case-insensitive):

| Method | Description |
| --- | --- |
| `Initialize(ПараметрыJSON)` | JSON string is forwarded to UAPKI `INIT` call. Example parameters are compatible with `library/test/data/sign-pkcs12-file.json`. |
| `SignFile(ПутьКФайлу, ПутьКПодписи, ПараметрыJSON)` | Signs a file, stores detached signature, returns JSON summary with `signAlgo`, `signingTime`, `signerCertId`, `certIds`, `signaturePath`. |
| `VerifyFileSignature(ПутьКФайлу, ПутьКПодписи, ПараметрыJSON)` | Verifies signature against file data, returns JSON structure identical to UAPKI `VERIFY`. |
| `GetCertificateInfo(Источник)` | Accepts either path to DER/PEM certificate or JSON object with fields supported by UAPKI `CERT_INFO`. |
| `SignData(Данные, ПараметрыJSON)` | Signs UTF-8 data stored in memory and returns Base64 signature (detached). |
| `VerifyDataSignature(Данные, ПодписьBase64, ПараметрыJSON)` | Verifies Base64 signature over in-memory data, returns JSON report. |

See [`examples/usage.bsl`](examples/usage.bsl) for a ready-to-run 1C script with `ПодписатьФайл` / `ПроверитьПодписьФайла` buttons.

## Error handling & logging

* All thrown C++ exceptions are translated to NativeAPI errors (the component method returns `Ложь`).
* The textual error is written to the `LastError` property so that diagnostics can be shown in the UI or recorded in the log.
* Optional logging can be enabled via UAPKI configuration (for example, pass `"debug":true` inside the initialization JSON).

## Delivery checklist

* `libuapki1c.so` / `uapki1c.dll` built for each target architecture.
* Runtime dependencies (`libuapki`, `libuapkif`, `libuapkic`, `cm-pkcs12`, etc.).
* This README + example `.bsl` script.
* Instructions for the target administrators on where to copy the binaries and how to reference them from configuration repositories.

