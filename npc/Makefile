NVBOARD_HOME ?= ../
TOPNAME = top
NXDC_FILES = constr/top.nxdc
INC_PATH ?=

VERILATOR = verilator
VERILATOR_CFLAGS += -MMD --build -cc -O3 --x-assign fast --x-initial fast --noassert

BUILD_DIR = ./build
OBJ_DIR = $(BUILD_DIR)/obj_dir
BIN = $(BUILD_DIR)/$(TOPNAME)

# 默认目标
default: $(BIN)

# 自动创建 build 目录
$(shell mkdir -p $(BUILD_DIR))

# constraint file 自动生成 auto_bind.cpp
SRC_AUTO_BIND = $(abspath $(BUILD_DIR)/auto_bind.cpp)
$(SRC_AUTO_BIND): $(NXDC_FILES)
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $^ $@

# project source
VSRCS = $(shell find $(abspath ./vsrc) -name "*.v")
CSRCS = $(shell find $(abspath ./csrc) -name "*.c" -or -name "*.cc" -or -name "*.cpp")
CSRCS += $(SRC_AUTO_BIND)

# NVBoard mk & SDL2
include $(NVBOARD_HOME)/scripts/nvboard.mk
LDFLAGS += $(NVBOARD_ARCHIVE) -lSDL2 -lSDL2_image

INCFLAGS = $(addprefix -I, $(INC_PATH))
CXXFLAGS += $(INCFLAGS) -DTOP_NAME="\"V$(TOPNAME)\""

# 编译可执行文件规则
$(BIN): $(VSRCS) $(CSRCS) $(NVBOARD_ARCHIVE)
	@rm -rf $(OBJ_DIR)
	$(VERILATOR) $(VERILATOR_CFLAGS) \
		--top-module $(TOPNAME) $^ \
		$(addprefix -CFLAGS , $(CXXFLAGS)) $(addprefix -LDFLAGS , $(LDFLAGS)) \
		--Mdir $(OBJ_DIR) --exe -o $(abspath $(BIN))

# sim target
sim: $(BIN)
	$(call git_commit, "sim RTL")
	@echo "==== 编译完成，开始运行仿真 ===="
	@./$(BIN)
	@echo "==== 仿真完成，波形文件生成在 build/ 目录下（如 wave.vcd 或 wave.fst）"

# all target 指向 default
all: default

# run target
run: $(BIN)
	@$(BIN)

# clean target
clean:
	rm -rf $(BUILD_DIR)

.PHONY: default all clean run sim

