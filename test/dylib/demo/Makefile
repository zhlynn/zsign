ARCHS = arm64
TARGET := iphone:clang:latest:7.0

include $(THEOS)/makefiles/common.mk

LIBRARY_NAME = demo

demo_FILES = demo.m
demo_CFLAGS = -fobjc-arc
demo_INSTALL_PATH = /usr/local/lib

include $(THEOS_MAKE_PATH)/library.mk
