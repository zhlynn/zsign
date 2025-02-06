#!/bin/bash

PACKAGES="../ipa"
PRIVATE_KEY="../assets/test.p12"
MOBILE_PROVISION="../assets/test.mobileprovision"

for file in "$PACKAGES"/*.ipa; do
    [ -e "$file" ] || continue

    echo -n "$file: "
    
    ../../bin/zsign -q -i -k $PRIVATE_KEY -m $MOBILE_PROVISION "$file" &>/dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo -e "\033[32mOK.\033[0m"
    else
        echo -e "\033[31m!!!FAILED!!!\033[0m"
    fi
done
