FROM alpine
WORKDIR /zsign
COPY . src/

RUN apk add --no-cache --virtual .build-deps g++ openssl-dev openssl-libs-static && \
	apk add --no-cache zip unzip && \
	g++ src/*.cpp src/common/*.cpp /usr/lib/libcrypto.a -O3 -o zsign -static -static-libgcc && \
	apk del .build-deps && \
	rm -rf src

ENTRYPOINT ["/zsign/zsign"]
CMD ["-v"]
