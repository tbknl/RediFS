CC = gcc
MKDIR = mkdir
RM = rm

LIBS = fuse hiredis

LIB_FLAGS = $(addprefix -l,$(LIBS))
CDEFINES = _FILE_OFFSET_BITS=64 FUSE_USE_VERSION=26
CFLAGS = -g -Wall
CFLAGS += $(addprefix -D,$(CDEFINES))

SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
DEP_DIR = $(BUILD_DIR)/dep

SRC_PATHS = $(wildcard $(SRC_DIR)/*.c)
OBJ_PATHS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_PATHS))


.PHONY: all

all: $(BUILD_DIR)/redifs


$(BUILD_DIR)/redifs: $(OBJ_PATHS) | $(BUILD_DIR)
	$(CC) $^ -o $@ $(LIB_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) $(DEP_DIR)
	$(CC) -c $< $(CFLAGS) -M -MF $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.o.d,$@) -MT $@
	$(CC) -c $< -o $@ $(CFLAGS)


$(BUILD_DIR) $(OBJ_DIR) $(DEP_DIR):
	$(MKDIR) -p $@


.PHONY: clean

clean:
	$(RM) -rf $(BUILD_DIR)


include $(wildcard $(DEP_DIR)/*.d)

