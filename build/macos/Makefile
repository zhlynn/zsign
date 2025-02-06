CXX = g++
CXXFLAGS = -std=c++11 -O3

ECHO := $(shell if echo -e "" | grep -q '^-e'; then echo "echo"; else echo "echo -e"; fi)

GREEN = \033[0;32m
NC = \033[0m

OPENSSL_INCLUDE = $(shell pkg-config --cflags openssl)
MINIZIP_NG_INCLUDE = $(shell pkg-config --cflags minizip)
OPENSSL_LIB = $(shell pkg-config --libs openssl)
MINIZIP_NG_LIB = $(shell pkg-config --libs minizip)

INCLUDES = -I../../src -I../../src/common
INCLUDES += $(OPENSSL_INCLUDE)
INCLUDES += $(MINIZIP_NG_INCLUDE)

LIBS = $(OPENSSL_LIB)
LIBS += $(MINIZIP_NG_LIB)

OBJDIR = .build

SRCS = $(wildcard ../../src/*.cpp) $(wildcard ../../src/common/*.cpp)
OBJS = $(SRCS:../../src/%.cpp=$(OBJDIR)/%.o)

TARGET = ../../bin/macos/zsign

all: $(TARGET)
	@$(ECHO) "$(GREEN)>>> Build OK!$(NC) -> $(abspath $(TARGET))"

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o $(TARGET)

$(OBJDIR)/%.o: ../../src/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/common/%.o: ../../src/common/%.cpp
	@mkdir -p $(OBJDIR)/common
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@$(ECHO) "$(GREEN)>>> Clean OK!$(NC)"

.PHONY: all clean
