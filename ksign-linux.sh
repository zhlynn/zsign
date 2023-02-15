#!/bin/bash

old_ipa="$1"
new_ipa="$2"
signAppId="$3"

if [ ! "$1" ] || [ ! "$2" ]  ; then
echo "Usage : ./ksign.sh  <需要签名的ipa路径>.ipa <签名后的ipa路径>.ipa signAppId(非必须)"
  exit 2
fi

if [ ! $signAppId ]; then
  ./zsign_linux -z 1  -n null  -k /opt/cert/crdis.p12 -m /opt/cert/ad_hoc.mobileprovision -x  -l libcklib.dylib -o $new_ipa  $old_ipa
else
  echo "signAppId is not null"
  ./zsign_linux -z 1  -n $signAppId  -k /opt/cert/crdis.p12 -m /opt/cert/ad_hoc.mobileprovision -x -l libcklib.dylib -o $new_ipa  $old_ipa
fi
