PYTHON ?= python3
CL65 ?= cl65
SIM65 ?= sim65
.PHONY: all ca65 d700 test legacy-test disk cart cart-test benchmark clean
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
cart:
	$(PYTHON) build.py --cart --output build-cart
	$(PYTHON) tools/make_cart.py --build build-cart
	$(PYTHON) build.py --cart --base 0xd700 --output build-cart-d700
	$(PYTHON) tools/make_cart.py --build build-cart-d700 --base 0xd700
cart-test: all cart
	PYTHONPATH=test $(PYTHON) -m unittest discover -s test -p test_cart.py -v
benchmark: all
	PYTHONPATH=test $(PYTHON) test/benchmark.py
clean:
	rm -rf build build-d700 build-cart build-cart-d700
