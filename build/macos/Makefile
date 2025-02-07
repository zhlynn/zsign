CXX = g++
CXXFLAGS = -std=c++11 -O3

ECHO := $(shell if echo -e "" | grep -q '^-e'; then echo "echo"; else echo "echo -e"; fi)
GREEN = \033[0;32m
NC = \033[0m

OPENSSL_INCLUDE = $(shell pkg-config --cflags openssl)
MINIZIP_INCLUDE = $(shell pkg-config --cflags minizip)
OPENSSL_LIB = $(shell pkg-config --libs openssl)
MINIZIP_LIB = $(shell pkg-config --libs minizip)

INCLUDES = -I../../src -I../../src/common
INCLUDES += $(OPENSSL_INCLUDE)
INCLUDES += $(MINIZIP_INCLUDE)

LIBS = $(OPENSSL_LIB)
LIBS += $(MINIZIP_LIB)

OBJDIR = .build
BINDIR = ../../bin

SRCS = $(wildcard ../../src/*.cpp) $(wildcard ../../src/common/*.cpp)
OBJS = $(SRCS:../../src/%.cpp=$(OBJDIR)/%.o)

TARGET = $(BINDIR)/zsign

all: $(TARGET)
	@$(ECHO) "$(GREEN)>>> Build OK!$(NC) -> $(abspath $(TARGET))"

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(OBJS) $(LIBS) -o $(TARGET)

$(OBJDIR)/%.o: ../../src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@$(ECHO) "$(GREEN)>>> Clean OK!$(NC)"

.PHONY: all clean
