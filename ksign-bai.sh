#!/bin/bash

old_ipa="$1"
new_ipa="$2"
signAppId="$3"

if [ ! "$1" ] || [ ! "$2" ]  ; then
echo "Usage : ./ksign.sh  <需要签名的ipa路径>.ipa <签名后的ipa路径>.ipa signAppId(非必须)"
  exit 2
fi

if [ ! $signAppId ]; then
  ./zsign -z 1  -n null  -k /Users/key/Desktop/Lab/cert/123123.p12 -p 123123 -m /Users/key/Desktop/Lab/cert/123123.mobileprovision -x  -l libcklib.dylib -o $new_ipa  $old_ipa
else
  echo "signAppId is not null"
  ./zsign -z 1  -n $signAppId  -k /Users/key/Desktop/Lab/cert/123123.p12 -p 123123 -m /Users/key/Desktop/Lab/cert/123123.mobileprovision -x -l libcklib.dylib -o $new_ipa  $old_ipa
fi
