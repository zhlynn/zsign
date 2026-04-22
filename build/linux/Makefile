CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wno-unused-result

ECHO := $(shell if echo -e "" | grep -q '^-e'; then echo "echo"; else echo "echo -e"; fi)
GREEN = \033[0;32m
NC = \033[0m

OPENSSL_INCLUDE = $(shell pkg-config --cflags openssl)
OPENSSL_LIB = $(shell pkg-config --libs openssl)

INCLUDES = -I../../src -I../../src/common -I../../src/third-party/zlib
INCLUDES += $(OPENSSL_INCLUDE)

# Version derived from git tag (strip leading 'v'); override with `make VERSION=x.y.z`
VERSION ?= $(shell git -C ../.. describe --tags --always 2>/dev/null | sed 's/^v//')
ifeq ($(strip $(VERSION)),)
VERSION := 0.0.0-dev
endif
CXXFLAGS += -DZSIGN_VERSION=$(VERSION)

LIBS = $(OPENSSL_LIB)

OBJDIR = .build
BINDIR = ../../bin

SRCS = $(wildcard ../../src/*.cpp) $(wildcard ../../src/common/*.cpp)
OBJS = $(SRCS:../../src/%.cpp=$(OBJDIR)/%.o)

ZLIB_SRCS = $(wildcard ../../src/third-party/zlib/*.c)
ZLIB_OBJS = $(ZLIB_SRCS:../../src/third-party/zlib/%.c=$(OBJDIR)/zlib/%.o)

MINIZIP_SRCS = ../../src/third-party/minizip/ioapi.c ../../src/third-party/minizip/zip.c ../../src/third-party/minizip/unzip.c
MINIZIP_OBJS = $(MINIZIP_SRCS:../../src/third-party/minizip/%.c=$(OBJDIR)/minizip/%.o)

TARGET = $(BINDIR)/zsign

all: $(TARGET)
	@$(ECHO) "$(GREEN)>>> Build OK!$(NC) -> $(abspath $(TARGET))"

$(TARGET): $(OBJS) $(ZLIB_OBJS) $(MINIZIP_OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(OBJS) $(ZLIB_OBJS) $(MINIZIP_OBJS) $(LIBS) -o $(TARGET)

$(OBJDIR)/%.o: ../../src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/zlib/%.o: ../../src/third-party/zlib/%.c
	@mkdir -p $(dir $@)
	$(CC) -O3 -Wno-unused-result -I../../src/third-party/zlib -c $< -o $@

$(OBJDIR)/minizip/%.o: ../../src/third-party/minizip/%.c
	@mkdir -p $(dir $@)
	$(CC) -O3 -Wno-unused-result $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@$(ECHO) "$(GREEN)>>> Clean OK!$(NC)"

.PHONY: all clean
