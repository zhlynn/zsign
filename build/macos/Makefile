CXX = g++
# -MMD -MP emits header dependency files so incremental builds rebuild every
# translation unit affected by a header change (e.g. class layout changes).
CXXFLAGS = -std=c++11 -O3 -MMD -MP

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

# `make SYSTEM_MINIZIP=1` links system minizip + zlib (found via `pkg-config
# minizip`) instead of compiling the vendored copies in src/third-party;
# `make SYSTEM_MINIZIP=ng` links minizip-ng through its minizip compat layer.
SYSTEM_MINIZIP ?= 0
ifneq ($(SYSTEM_MINIZIP),0)
ifeq ($(SYSTEM_MINIZIP),ng)
MINIZIP_PC = minizip-ng
CXXFLAGS += -DZSIGN_SYSTEM_MINIZIP_NG
else
MINIZIP_PC = minizip
CXXFLAGS += -DZSIGN_SYSTEM_MINIZIP
endif
INCLUDES = -I../../src -I../../src/common
INCLUDES += $(OPENSSL_INCLUDE) $(shell pkg-config --cflags $(MINIZIP_PC))
LIBS += $(shell pkg-config --libs $(MINIZIP_PC))
ZLIB_OBJS =
MINIZIP_OBJS =
endif

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
	$(CC) -O3 -I../../src/third-party/zlib -c $< -o $@

$(OBJDIR)/minizip/%.o: ../../src/third-party/minizip/%.c
	@mkdir -p $(dir $@)
	$(CC) -O3 $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@$(ECHO) "$(GREEN)>>> Clean OK!$(NC)"

-include $(OBJS:.o=.d)

.PHONY: all clean
