FROM alpine
WORKDIR /zsign
COPY . src/

RUN apk add --no-cache --virtual .build-deps g++ clang clang-static openssl-dev openssl-libs-static && \
    apk add --no-cache zip unzip && \
    clang++ src/*.cpp src/common/*.cpp /usr/lib/libcrypto.a -O3 -o zsign -static && \
	apk del .build-deps && \
    rm -rf src

ENTRYPOINT ["/zsign/zsign"]
CMD ["-v"]
