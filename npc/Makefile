# ================================
# NPC + NVBoard + STA Makefile
# ================================

# -----------------------------
# 路径配置
# -----------------------------
PROJ_PATH := /home/guoyuchen/ysyx/ysyx-workbench/npc
SCRIPT_DIR := /home/guoyuchen/ysyx/yosys-sta/scripts
SDC_FILE := $(SCRIPT_DIR)/default.sdc

# -----------------------------
# NVBoard / Verilator 配置
# -----------------------------
TOPNAME = top
NXDC_FILES = constr/top.nxdc
INC_PATH ?=
VERILATOR = verilator
VERILATOR_CFLAGS += -MMD --build -cc \
    -O3 --x-assign fast --x-initial fast --noassert \
    -Wall -Wextra -Wpedantic -Wimplicit

BUILD_DIR = ./build
OBJ_DIR = $(BUILD_DIR)/obj_dir
BIN = $(BUILD_DIR)/$(TOPNAME)

# 自动创建 build 目录
$(shell mkdir -p $(BUILD_DIR))

# 自动生成 pin 绑定文件
SRC_AUTO_BIND = $(abspath $(BUILD_DIR)/auto_bind.cpp)
$(SRC_AUTO_BIND): $(NXDC_FILES)
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $^ $@

# 项目源文件
VSRCS = $(shell find $(abspath ./vsrc) -name "*.v")
CSRCS = $(shell find $(abspath ./csrc) -name "*.c" -or -name "*.cc" -or -name "*.cpp")
CSRCS += $(SRC_AUTO_BIND)

# 包含 NVBoard 编译规则
include $(NVBOARD_HOME)/scripts/nvboard.mk

# Verilator include flags
INCFLAGS = $(addprefix -I, $(INC_PATH))
CXXFLAGS += $(INCFLAGS) -DTOP_NAME="\"V$(TOPNAME)\""

# -----------------------------
# NPC + STA 配置
# -----------------------------
O ?= $(PROJ_PATH)/result
DESIGN ?= top
CLK_FREQ_MHZ ?= 500
CLK_PORT_NAME ?= clk
PDK ?= nangate45
RTL_FILES ?= $(shell find $(PROJ_PATH)/vsrc -name "*.v")

RESULT_DIR := $(O)/$(DESIGN)-$(CLK_FREQ_MHZ)MHz
NETLIST_SYN_V   := $(RESULT_DIR)/$(DESIGN).netlist.syn.v
NETLIST_FIXED_V := $(RESULT_DIR)/$(DESIGN).netlist.fixed.v
TIMING_RPT      := $(RESULT_DIR)/$(DESIGN).rpt

# -----------------------------
# 规则定义
# -----------------------------

# 初始化 STA 环境
init:
	bash -c "$$(wget -O - https://ysyx.oscc.cc/slides/resources/scripts/init-yosys-sta.sh)"

# 编译 NVBoard/Verilator 仿真程序
$(BIN): $(VSRCS) $(CSRCS) $(NVBOARD_ARCHIVE)
	@rm -rf $(OBJ_DIR)
	$(VERILATOR) $(VERILATOR_CFLAGS) \
		--top-module $(TOPNAME) $^ \
		$(addprefix -CFLAGS , $(CXXFLAGS)) $(addprefix -LDFLAGS , $(LDFLAGS)) \
		--Mdir $(OBJ_DIR) --exe -o $(abspath $(BIN))

# 综合规则
syn: $(NETLIST_SYN_V)

$(NETLIST_SYN_V): $(RTL_FILES) $(SCRIPT_DIR)/yosys.tcl
	mkdir -p $(@D)
	echo tcl $(SCRIPT_DIR)/yosys.tcl $(DESIGN) $(PDK) \"$(RTL_FILES)\" $@ | yosys -l $(@D)/yosys.log -s -

# 修复扇出
fix-fanout: $(NETLIST_FIXED_V)

$(NETLIST_FIXED_V): $(SCRIPT_DIR)/fix-fanout.tcl $(SDC_FILE) $(NETLIST_SYN_V)
	set -o pipefail && ./bin/iEDA -script $^ $(DESIGN) $(PDK) $@ 2>&1 | tee $(RESULT_DIR)/fix-fanout.log
	echo tcl $(SCRIPT_DIR)/yosys-area.tcl $(DESIGN) $(PDK) $@ | yosys -l $(@D)/yosys-fixed.log -s -

# 时序分析
sta: $(TIMING_RPT)

$(TIMING_RPT): $(SCRIPT_DIR)/sta.tcl $(SDC_FILE) $(NETLIST_FIXED_V)
	set -o pipefail && ./bin/iEDA -script $^ $(DESIGN) $(PDK) 2>&1 | tee $(RESULT_DIR)/sta.log

sim:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@echo "Write this Makefile by your self."

# -----------------------------
# 常规命令
# -----------------------------
all: default

default: $(BIN)

run: $(BIN)
	@$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(O)

.PHONY: init syn fix-fanout sta sim default all run clean

