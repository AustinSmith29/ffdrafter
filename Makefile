TARGET_EXEC := fdraft

BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -name '*.c')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CC = gcc
CFLAGS := $(INC_FLAGS) -MMD -MP -Wall -g
LDLIBS = -lm

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) `pkg-config --cflags libconfig` $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS) \
		`pkg-config --libs libconfig`
	cp $(BUILD_DIR)/$(TARGET_EXEC) .

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ 
.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/*

-include $(DEPS)
