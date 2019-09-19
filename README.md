# zsign
maybe the most quickly codesign alternative for ios12+ in the world,  cross-platform, more features.

### Complie
I have already tested on mac and linux, but windows is coming soon.
```bash
g++ *.cpp common/*.cpp -lssl -lcrypto -o zsign -O3
```

### Usage
```bash
./zsign -f -c cert.pem -k privkey.pem -m dev.mobileprovisioning -o resign.ipa demo.ipa 
```
