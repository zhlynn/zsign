FROM centos:7
WORKDIR /zsign
RUN yum -y install gcc gcc-c++ openssl-devel zip unzip
COPY . .
RUN g++ *.cpp common/*.cpp -lcrypto -O3 -o zsign
ENTRYPOINT [ "/zsign/zsign" ]
CMD [ "/zsign/zsign", "-v" ]