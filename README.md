# zsign — Fast Cross-Platform iOS Code Signing Tool

[![Build](https://github.com/zhlynn/zsign/actions/workflows/build.yml/badge.svg)](https://github.com/zhlynn/zsign/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows%20%7C%20Android%20%7C%20FreeBSD-lightgrey)](#build)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-00599C.svg)](#build)
[![Stars](https://img.shields.io/github/stars/zhlynn/zsign?style=social)](https://github.com/zhlynn/zsign)

**Languages:** English | [简体中文](README.zh-CN.md)

**zsign** is a fast, open-source, cross-platform `codesign` alternative for iOS 12+. It re-signs `.ipa` packages, Mach-O binaries, and `.app` bundles with custom certificates and provisioning profiles — without Xcode, without macOS. Ideal for iOS app re-signing, dylib injection, CI/CD pipelines, and IPA distribution workflows on Linux and Windows servers.

**Keywords:** iOS code signing · codesign alternative · re-sign IPA · Mach-O signing · dylib injection · iOS CI/CD · Linux iOS signing · Windows iOS signing · p12 signing · provisioning profile · OCSP check · ad-hoc signature

If this tool helps you, please give it a ⭐ **star** — [zhlynn](https://github.com/zhlynn)

## Table of Contents

- [Features](#features)
- [Supported Platforms](#supported-platforms)
- [Build](#build)
- [Usage](#usage)
- [Examples](#examples)
- [Certificate Check (-C)](#certificate-check--c)
- [Fast Re-signing](#fast-re-signing)
- [License](#license)

## Features

- **Fast IPA re-signing** with persistent `.zsign_cache` — subsequent signs skip unchanged Mach-Os
- **Cross-platform** — runs on macOS, Linux, Windows, Android, and FreeBSD (no Xcode required)
- **Dylib injection / removal** — `LC_LOAD_DYLIB` and `LC_LOAD_WEAK_DYLIB` support
- **Multi-profile signing** — per-extension provisioning profiles for apps with extensions
- **Bundle editing** — change bundle ID, display name, version, minimum OS version, entitlements
- **Ad-hoc signing** — sign without a developer certificate
- **Certificate / OCSP check** — inspect certificates in `.ipa`, `.p12`, `.mobileprovision`, `.cer`, `.pem`, or Mach-O
- **Metadata extraction** — pull `Info.plist` fields and app icon to `metadata.json` + PNG
- **Bundle cleanup** — remove app extensions, watch apps, `UISupportedDevices`
- **Files app integration** — toggle `UISupportsDocumentBrowser` and `UIFileSharingEnabled`

## Supported Platforms

macOS · Linux · Windows · Android · FreeBSD

## Build

### macOS

```bash
brew install pkg-config openssl
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/macos
make clean && make
```

### Linux

#### Ubuntu / Debian

```bash
sudo apt-get install -y git g++ pkg-config libssl-dev
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/linux
make clean && make
```

#### RHEL / CentOS / Alma / Rocky

Install `epel-release` first:

```bash
sudo yum -y install epel-release
```

Then build:

```bash
sudo yum install -y git gcc-c++ pkg-config openssl-devel
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/linux
make clean && make
```

### Windows

Open `build/windows/vs2022/zsign.sln` in Visual Studio 2022 and build.

## Usage

```
Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder

Options:
  -k, --pkey              Path to private key or p12 file (PEM or DER format)
  -m, --prov              Path to provisioning profile (use multiple -m for extensions)
  -c, --cert              Path to certificate file (PEM or DER format)
  -a, --adhoc             Perform ad-hoc signature only
  -d, --debug             Generate debug output (.zsign_debug folder)
  -f, --force             Force sign without cache
  -o, --output            Path to output ipa file
  -p, --password          Password for private key or p12 file
  -b, --bundle_id         New bundle identifier
  -n, --bundle_name       New bundle display name
  -r, --bundle_version    New bundle version
  -e, --entitlements      New entitlements file
  -z, --zip_level         Compression level for output ipa (0-9)
  -l, --dylib             Dylib to inject (use multiple -l for multiple dylibs)
  -D, --rm_dylib          Dylib to remove (use multiple -D for multiple)
  -w, --weak              Inject dylib as LC_LOAD_WEAK_DYLIB
  -i, --install           Install via ideviceinstaller after signing
  -t, --temp_folder       Temporary folder for intermediate files
  -2, --sha256_only       Use SHA256-only code directory
  -C, --check             Check certificate validity and OCSP status
  -x, --metadata          Extract metadata and icon to directory
  -R, --rm_provision      Remove provisioning profile after signing
  -S, --enable_docs       Enable UISupportsDocumentBrowser and UIFileSharingEnabled
  -M, --min_version       Set MinimumOSVersion in Info.plist
  -E, --rm_extensions     Remove all app extensions (PlugIns/Extensions)
  -W, --rm_watch          Remove watch app from bundle
  -U, --rm_uisd           Remove UISupportedDevices from Info.plist
  -q, --quiet             Quiet operation
  -v, --version           Show version
  -h, --help              Show help
```

## Examples

**Show Mach-O and codesignature info:**
```bash
zsign demo.app/demo
```

**Sign an IPA:**
```bash
zsign -k privkey.pem -m dev.prov -o output.ipa -z 9 demo.ipa
```

**Sign with p12 (cached):**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -o output.ipa demo.app
```

**Sign with p12 (force, no cache):**
```bash
zsign -f -k dev.p12 -p 123 -m dev.prov -o output.ipa demo.app
```

**Ad-hoc sign:**
```bash
zsign -a -o output.ipa demo.ipa
```

**Inject dylib and re-sign:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -l demo.dylib -o output.ipa demo.ipa
```

**Change bundle id and name:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -b 'com.new.bundle.id' -n 'NewName' -o output.ipa demo.ipa
```

**Inject dylib (LC_LOAD_DYLIB) into Mach-O:**
```bash
zsign -a -l "@executable_path/demo1.dylib" -l "@executable_path/demo2.dylib" demo.app/execute
```

**Inject weak dylib (LC_LOAD_WEAK_DYLIB):**
```bash
zsign -w -l "@executable_path/demo.dylib" demo.app/execute
```

**Extract metadata and icon:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -x ./metadata -o output.ipa demo.ipa
# outputs ./metadata/metadata.json and ./metadata/<hash>.png
```

**Enable Files app integration:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -S -o output.ipa demo.ipa
```

**Set minimum OS version:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -M 14.0 -o output.ipa demo.ipa
```

**Remove app extensions:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -E -o output.ipa demo.ipa
```

**Remove watch app:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -W -o output.ipa demo.ipa
```

**Remove UISupportedDevices:**
```bash
zsign -k dev.p12 -p 123 -m dev.prov -U -o output.ipa demo.ipa
```

## Certificate Check (-C)

Check the signing certificate of any supported file and perform an OCSP revocation check against Apple's servers. Reads binaries directly from inside IPA files without extracting to disk.

**Supported file types:** `.ipa`, `.mobileprovision`, `.p12`/`.pfx`, `.cer`/`.pem`, Mach-O binaries

```bash
# Check an IPA
zsign -C demo.ipa

# Check a provisioning profile
zsign -C dev.mobileprovision

# Check a P12/PFX certificate
zsign -C dev.p12 -p 123

# Check a Mach-O binary
zsign -C demo.app/demo

# Sign and verify certificate before archiving
zsign -C -k dev.p12 -p 123 -m dev.prov -o output.ipa demo.ipa
```

**Example output:**
```
>>> Check:      demo.ipa (IPA)
>>> Signed:     Yes
>>> Name:       Apple Distribution: Company Name (TEAMID)
>>> Type:       Apple Distribution
>>> Org:        Company Name
>>> Team:       TEAMID
>>> Serial:     XX:XX:XX:XX:XX:XX:XX:XX
>>> Issued:     2025-01-01T00:00:00Z
>>> Expires:    2026-01-01T00:00:00Z (365 days remaining)
>>> Algorithm:  RSA 2048-bit
>>> Issuer:     Apple Worldwide Developer Relations Certification Authority
>>> OCSP:       Valid (ocsp.apple.com)
```

## Fast Re-signing

Unzip the IPA first, then sign the extracted folder. On the first sign, zsign caches signature data in `.zsign_cache`. Subsequent re-signs with different assets reuse the cache, making the process significantly faster — a key advantage over running `codesign` from scratch on every build.

## FAQ

**Q: Does zsign require macOS or Xcode?**
No. zsign runs natively on Linux, Windows, macOS, Android, and FreeBSD without Xcode or Apple's `codesign` binary.

**Q: Can I use zsign in CI/CD pipelines?**
Yes. zsign is a single static-linkable binary designed for automation — Linux runners, Docker images, and Windows build agents all work.

**Q: Does zsign support SHA256-only code directories?**
Yes, via `-2` / `--sha256_only`.

**Q: How does zsign compare to Apple's `codesign`?**
zsign is cross-platform, scriptable on non-Apple hosts, supports dylib injection out of the box, and uses a signature cache for fast re-signing.

## License

zsign is licensed under the MIT License. See the [LICENSE](LICENSE) file.
