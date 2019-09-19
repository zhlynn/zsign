# zsign
Maybe is the most quickly codesign alternative for ios12+ in the world,  cross-platform, more features.

### Compile
You must install openssl library at first, then
```bash
g++ *.cpp common/*.cpp -lssl -lcrypto -o zsign -O3
```

### Usage
I have already tested on mac and linux(need unzip and zip command).
```bash
usage: ./zsign -c cert.pem -k privkey.pem -m dev.mobileprovisioning -o output.ipa [options]... [file|folder]
options:
-k, --pkey          Path to private key or p12 file. (PEM or DER format)
-c, --cert          Path to certificate file. (PEM or DER format)
-d, --debug         Generate debug output files. (.zsign_debug folder)
-f, --force         Force signing without cache when sign folder.
-o, --output        Path to output ipa file.
-p, --password      Password for private key or p12 file.
-b, --bundleid      New bundle id to change.
-n, --bundlename    New bundle name to change.
-e, --entitlements  New entitlements to change.
-z, --ziplevel      Compressed level when output the ipa file. (0-9)
-l, --dylib         Path to inject dylib file.
-i, --install       Install ipa file using ideviceinstaller command for test.
-q, --quiet         Quiet operation.
-v, --version       Verbose for version
-h, --help          Show help.
```


1. Sign IPA with privatekey and mobileprovisioning file.
```bash
./zsign -k privkey.pem -m dev.mobileprovisioning -o resign.ipa -z 9 demo.ipa
```

2. Sign Folder with p12 and mobileprovisioning file.
```bash
./zsign -k dev.p12 -p 123 -m dev.mobileprovisioning -o resign.ipa folder
```

3. Inject dylib and resign.
```bash
./zsign -k dev.p12 -p 123 -m dev.mobileprovisioning -o resign.ipa demo.ipa -l demo.dylib
```
