# zsign
maybe the most quickly codesign alternative for ios12+ in the world,  cross-platform, more features.

### Complie
you must install openssl library at first, then
```bash
g++ *.cpp common/*.cpp -lssl -lcrypto -o zsign -O3
```

### Usage
I have already tested on mac and linux(need unzip and zip command), windows is coming soon.
```bash
./zsign -f -c cert.pem -k privkey.pem -m dev.mobileprovisioning -o resign.ipa demo.ipa 
```
