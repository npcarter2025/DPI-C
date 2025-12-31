# Makefile for compiling and running DPI-C transaction system
# RTL -> XACTOR -> C++ -> SPARSE MEMORY

# VCS compiler
VCS = vcs

# VCS compilation options
# -cpp g++ tells VCS to use g++ for C++ files
# -cc specifies C compiler (needed for DPI)
VCS_OPTIONS = -sverilog -full64 -debug_all -timescale=1ns/1ns -cpp g++ -cc gcc

# Source files
SV_FILES = top_tb.sv rtl_master.sv xactor.sv
CPP_FILE = dpi_xactor.cpp

# Output files
SIMV = simv

# Default target
all: run

# Compile SystemVerilog with DPI-C
# VCS automatically compiles .cpp files when included in the command
compile: $(SV_FILES) $(CPP_FILE)
	@echo "Compiling SystemVerilog with DPI-C..."
	$(VCS) $(VCS_OPTIONS) $(SV_FILES) $(CPP_FILE) -l compile.log -o $(SIMV)
	@if [ $$? -eq 0 ]; then \
		echo "Compilation successful!"; \
	else \
		echo "Compilation failed. Check compile.log for errors."; \
		exit 1; \
	fi

# Run simulation
run: compile
	@echo ""
	@echo "Running simulation..."
	@echo "===================="
	LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(SIMV) -l run.log
	@echo "===================="
	@echo "Simulation complete. Check run.log for details."

# Clean up generated files
clean:
	rm -rf $(SIMV) simv.daidir csrc ucli.key vc_hdrs.h DVEfiles inter.vpd *.log simv.vdb *.vpd *.o

# Help message
help:
	@echo "Makefile for DPI-C transaction system"
	@echo ""
	@echo "Usage:"
	@echo "  make compile  - Compile C++ library and SystemVerilog"
	@echo "  make run      - Compile and run simulation"
	@echo "  make clean    - Clean up generated files"
	@echo "  make help     - Display this help message"
	@echo ""
	@echo "Default target: run"

.PHONY: compile run clean help all

