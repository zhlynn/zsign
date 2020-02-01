FROM alpine
WORKDIR /zsign
COPY . src/

RUN apk add --no-cache --virtual .build-deps g++ openssl-dev && \
	apk add --no-cache libgcc libstdc++ zip unzip && \
	g++ src/*.cpp src/common/*.cpp -lcrypto -O3 -o zsign && \
	apk del .build-deps && \
	rm -rf src

ENTRYPOINT ["/zsign/zsign"]
CMD ["-v"]
