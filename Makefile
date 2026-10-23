PYTHON ?= python3
CL65 ?= cl65
SIM65 ?= sim65
.PHONY: all ca65 d700 test legacy-test disk benchmark clean
all: ca65 d700
ca65:
	$(PYTHON) build.py
d700:
	$(PYTHON) build.py --base 0xd700 --output build-d700
test: all legacy-test
	PYTHONPATH=test $(PYTHON) -m unittest discover -s test -v
legacy-test:
	mkdir -p build
	$(CL65) -t sim6502 --asm-include-dir . -o build/scroll_test.bin test/sim65/scroll_test.s
	$(SIM65) build/scroll_test.bin
	$(CL65) -t sim6502 --asm-include-dir . -o build/atascii_test.bin test/sim65/atascii_test.s
	$(SIM65) build/atascii_test.bin
disk: all
	$(PYTHON) tools/make_disk.py
benchmark: all
	PYTHONPATH=test $(PYTHON) test/benchmark.py
clean:
	rm -rf build build-d700
