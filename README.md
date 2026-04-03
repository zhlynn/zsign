# zsign

A fast, cross-platform codesign alternative for iOS 12+. Re-sign iOS apps (.ipa, Mach-O, .app bundles) with custom certificates and provisioning profiles.

**Supported platforms:** macOS, Linux, Windows, Android, FreeBSD

If this tool helps you, please give it a ⭐ **star** — [zhlynn](https://github.com/zhlynn)

## Build

### macOS

```bash
brew install pkg-config openssl minizip-ng
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/macos
make clean && make
```

### Linux

#### Ubuntu / Debian

```bash
sudo apt-get install -y git g++ pkg-config libssl-dev libminizip-ng-dev
git clone https://github.com/zhlynn/zsign.git
cd zsign/build/linux
make clean && make
```

#### RHEL / CentOS / Alma / Rocky

Install `epel-release` first:

```bash
# RHEL / CentOS / Alma / Rocky 8
sudo rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm

# RHEL / CentOS / Alma / Rocky 9
sudo rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
```

Then build:

```bash
sudo yum install -y git gcc-c++ pkg-config openssl-devel minizip-ng-devel
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

Unzip the IPA first, then sign the extracted folder. On the first sign, zsign caches signature data in `.zsign_cache`. Subsequent re-signs with different assets reuse the cache, making the process significantly faster.

## License

zsign is licensed under the MIT License. See the [LICENSE](LICENSE) file.
