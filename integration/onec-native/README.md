# Native 1C component on top of UAPKI

This directory contains the source code of a cross-platform NativeAPI component for **1C:Enterprise 8** that wraps the UAPKI JSON API and exposes high-level methods for signing and verifying data with **DSTU 4145-2002** (and other algorithms supported by UAPKI).

## Features

* Works both on Windows and Linux thick clients of 1C (builds shared library `uapki1c.dll`/`libuapki1c.so`).
* Uses the official UAPKI JSON API (`process()`/`json_free()`) and keeps all crypto logic inside the proven library.
* Methods exported to 1C:
  * `Initialize(ПараметрыJSON)` – forwards the provided configuration directly to UAPKI `INIT`.
  * `SignFile(ПутьКФайлу, ПутьКПодписи, ПараметрыJSON)` – reads a file, signs it (default CAdES-BES, DSTU 4145-2002), stores detached signature and returns JSON summary with signer/certificate info.
  * `VerifyFileSignature(ПутьКФайлу, ПутьКПодписи, ПараметрыJSON)` – verifies detached/attached signature, returns JSON report (`signatureInfos`, certificate chain, timestamps).
  * `GetCertificateInfo(Источник)` – accepts either a path to certificate file or JSON object and returns serial, subject, issuer, validity, signature algorithm.
  * `SignData(Данные, ПараметрыJSON)` – signs an in-memory string/byte array and returns Base64 of the signature.
  * `VerifyDataSignature(Данные, ПодписьBase64, ПараметрыJSON)` – verifies Base64 signature over in-memory data, returns JSON report.
* Property `LastError` stores the textual description of the last exception so that `Попытка … Исключение` blocks in 1C can show human-readable diagnostics.

## Build instructions

1. Build UAPKI for your target platform using the existing scripts from the `library/` directory (`build-uapki.sh`, Visual Studio solution, etc.). After the build you need the headers (`uapki-export.h`) and the dynamic libraries (`uapki`, `uapkif`, `uapkic`, optional `cm-*`).
2. Configure and build the component:

```bash
cd integration/onec-native
cmake -B build -DUAPKI_ROOT=/absolute/path/to/uapki/install
cmake --build build --config Release
```

On Windows pass the generator and architecture explicitly, for example `-G "Visual Studio 17 2022" -A x64`. The resulting binaries will appear in `build/` (DLL on Windows, SO on Linux).

3. Copy the built `uapki1c.dll`/`libuapki1c.so` together with UAPKI runtime libraries to a folder that is accessible for the 1C platform:
   * **Windows** – place the DLLs next to the 1C executable or in a folder listed in `%PATH%`.
   * **Linux** – copy `libuapki1c.so` and the UAPKI shared objects to `/opt/uapki/` (for example) and add the directory to `LD_LIBRARY_PATH` or `/etc/ld.so.conf.d/uapki.conf` followed by `sudo ldconfig`.

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

