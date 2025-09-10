all:
	@echo "Write this Makefile by your self."

sim:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

	verilator -Wall --cc ../top.v --exe ../sim_main.cpp --trace --build
	@echo "==== 编译完成，开始运行仿真 ===="
	./obj_dir/Vtop
	@echo "==== 仿真完成，波形文件生成在 obj_dir/ 目录下（如 wave.vcd 或 wave.fst）"
include ../Makefile
