#!/bin/bash

old_ipa="$1"
new_ipa="$2"
signAppId="$3"

if [ ! "$1" ] || [ ! "$2" ]  ; then
echo "Usage : ./mysign.sh  <需要签名的ipa路径>.ipa <签名后的ipa路径>.ipa signAppId(非必须)"
  exit 2
fi

if [ ! $signAppId ]; then
  ./zsign -z 1  -n null  -k /Users/key/Desktop/My/ios/cert/cr/dis.p12 -m /Users/key/Desktop/My/ios/cert/cr/ad_hoc.mobileprovision -x  -l /Users/key/Library/Developer/Xcode/DerivedData/cklib-dosapgmlmdgeznetupmgmmyuyvxj/Build/Products/Debug-iphoneos/libcklib.dylib -l libcklib.dylib -o $new_ipa  $old_ipa
else
  echo "signAppId is not null"
  ./zsign -z 1  -n $signAppId  -k /Users/amu/Documents/my/code/mac/cert/cr/cr_dev.p12 -m /Users/amu/Documents/my/code/mac/cert/cr/cr_dev.mobileprovision -x  -l /Users/key/Library/Developer/Xcode/DerivedData/cklib-dosapgmlmdgeznetupmgmmyuyvxj/Build/Products/Debug-iphoneos/libcklib.dylib -l libcklib.dylib -o $new_ipa  $old_ipa
fi
