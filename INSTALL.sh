#!/bin/bash

MacOs=darwin
CentOs=yum
Ubuntu=apt-get
CmakeV=3.21.3
zsign=https://github.com/zhlynn/zsign.git

# Compile on CentOS
if [[ "$OSTYPE" =~ ^$MacOs ]]; then
echo "$OSTYPE is Mac"


# Compile on CentOS

    # Detect OS if it is CentOS
    elif [ -x /usr/bin/$CentOs ]; then

    # Clone zsign from zhlynn github repository!
rm -rf zsign; git clone $zsign &&
cd zsign &&

        # Dependencies
        yum install openssl-devel -y;
        yum install wget zip unzip -y &&
        yum group install "Development Tools" -y &&

            # Installing Cmake latest
wget -qO- "https://cmake.org/files/v3.21/cmake-$CmakeV-linux-x86_64.tar.gz" | \
tar --strip-components=1 -xz -C /usr/local &&



# Compile zsign using cmake
mkdir build; cd build &&
cmake .. &&
make


# Compile on Ubuntu

    # Detect OS if it is ubuntu
    elif [ -x /usr/bin/$Ubuntu ]; then

# Clone zsign from zhlynn github repository!
rm -rf zsign; git clone $zsign &&
cd zsign &&

        # Dependencies
        sudo apt-get install wget zip unzip build-essential checkinstall zlib1g-dev libssl-dev -y &&

            # Installing Cmake latest
            wget -qO- "https://cmake.org/files/v3.21/cmake-$CmakeV-linux-x86_64.tar.gz" | \
            tar --strip-components=1 -xz -C /usr/local &&
            rm -rf zsign &&


# Compile zsign using cmake
mkdir build; cd build &&
cmake .. &&
make
    fi
