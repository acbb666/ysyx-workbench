all:
	@echo "Write this Makefile by your self."

sim:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@echo "Write this Makefile by your self."

include ../Makefile

# ===============================
# 项目构建变量
# ===============================
TOPNAME = top
NXDC_FILES = constr/top.nxdc
INC_PATH ?=

VERILATOR = verilator
VERILATOR_CFLAGS += -MMD --build -cc \
				-O3 --x-assign fast --x-initial fast --noassert

BUILD_DIR = ./build
OBJ_DIR = $(BUILD_DIR)/obj_dir
BIN = $(BUILD_DIR)/$(TOPNAME)

# ===============================
# 默认目标
# ===============================
default: $(BIN)

# 创建构建目录
$(shell mkdir -p $(BUILD_DIR))

# ===============================
# 自动生成约束绑定文件
# ===============================
SRC_AUTO_BIND = $(abspath $(BUILD_DIR)/auto_bind.cpp)
$(SRC_AUTO_BIND): $(NXDC_FILES)
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $^ $@

# ===============================
# 项目源文件
# ===============================
VSRCS = $(shell find $(abspath ./vsrc) -name "*.v")
CSRCS = $(shell find $(abspath ./csrc) -name "*.c" -or -name "*.cc" -or -name "*.cpp")
CSRCS += $(SRC_AUTO_BIND)

# ===============================
# NVBoard 规则
# ===============================
include $(NVBOARD_HOME)/scripts/nvboard.mk

# ===============================
# 编译选项
# ===============================
INCFLAGS = $(addprefix -I, $(INC_PATH))
CXXFLAGS += $(INCFLAGS) -DTOP_NAME="\"V$(TOPNAME)\""

# ===============================
# 添加 NVBoard include 路径，确保 sim_main.cpp 能找到 nvboard.h
# ===============================
NVBOARD_INCLUDE = -I$(NVBOARD_HOME)/usr/include

# 将 NVBoard include 加入 CXXFLAGS
CXXFLAGS += $(NVBOARD_INCLUDE)

# 也可以加入 Verilator 编译选项，防止找不到头文件
VERILATOR_CFLAGS += $(NVBOARD_INCLUDE)

# ===============================
# 构建可执行仿真程序
# ===============================
$(BIN): $(VSRCS) $(CSRCS) $(NVBOARD_ARCHIVE)
	@rm -rf $(OBJ_DIR)
	$(VERILATOR) $(VERILATOR_CFLAGS) \
		--top-module $(TOPNAME) $^ \
		$(addprefix -CFLAGS , $(CXXFLAGS)) $(addprefix -LDFLAGS , $(LDFLAGS)) \
		--Mdir $(OBJ_DIR) --exe -o $(abspath $(BIN))

# ===============================
# 常用目标
# ===============================
all: default

run: $(BIN)
	@echo "Running simulation..."
	$(BIN)
	
clean:
	rm -rf $(BUILD_DIR)

.PHONY: default all clean run sim

