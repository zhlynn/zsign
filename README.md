# zsign
Maybe the most quickly codesign alternative for ios12+ in the world,  cross-platform, more features.

### Complie
You must install openssl library at first, then
```bash
g++ *.cpp common/*.cpp -lssl -lcrypto -o zsign -O3
```

### Usage
I have already tested on mac and linux(need unzip and zip command), windows is coming soon.
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
