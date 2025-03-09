all : flash

MINICHLINK:=$(shell dirname $(shell which minichlink))
TARGET:=firmware

CH32V003FUN:=./ch32fun

TARGET_MCU?=CH32V003
include ./ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
