DST_NAME = tiantian
SRCS = main.c board.c
LDSCRIPT=$(TOOLCHAIN_DIR)/$(ARCH_NAME-y).ld

include $(TOOLCHAIN_DIR)/targets.mk
include $(TOOLCHAIN_DIR)/rules.mk
