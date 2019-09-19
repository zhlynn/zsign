# zsign
Maybe is the most quickly codesign alternative for iOS12+ in the world, cross-platform, more features.

### Compile
You must install openssl library at first, then
```bash
g++ *.cpp common/*.cpp -lssl -lcrypto -O3 -o zsign
```

### Usage
I have already tested on macOS and Linux (need unzip and zip command).
```bash
Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder

options:
-k, --pkey          Path to private key or p12 file. (PEM or DER format)
-m, --prov          Path to mobile provisioning profile.
-c, --cert          Path to certificate file. (PEM or DER format)
-d, --debug         Generate debug output files. (.zsign_debug folder)
-f, --force         Force sign without cache when signing folder.
-o, --output        Path to output ipa file.
-p, --password      Password for private key or p12 file.
-b, --bundleid      New bundle id to change.
-n, --bundlename    New bundle name to change.
-e, --entitlements  New entitlements to change.
-z, --ziplevel      Compressed level when output the ipa file. (0-9)
-l, --dylib         Path to inject dylib file.
-i, --install       Install ipa file using ideviceinstaller command for test.
-q, --quiet         Quiet operation.
-v, --version       Show version.
-h, --help          Show help.
```

1. Show CodeSignature segment info.
```bash
./zsign WeChat.app/WeChat
```

2. Sign ipa with private key and mobileprovisioning file.
```bash
./zsign -k privkey.pem -m dev.prov -o output.ipa -z 9 demo.ipa
```

3. Sign folder with p12 and mobileprovisioning file (using cache).
```bash
./zsign -k dev.p12 -p 123 -m dev.prov -o output.ipa folder
```

4. Sign folder with p12 and mobileprovisioning file (without cache).
```bash
./zsign -f -k dev.p12 -p 123 -m dev.prov -o output.ipa folder
```

5. Inject dylib and resign.
```bash
./zsign -k dev.p12 -p 123 -m dev.prov -o output.ipa -l demo.dylib demo.ipa 
```

6. Change bundle id and bundle name
```bash
./zsign -k dev.p12 -p 123 -m dev.prov -o output.ipa -b 'com.tree.new.bee' -n 'TreeNewBee' demo.ipa
```