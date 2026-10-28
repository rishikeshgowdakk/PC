.PHONY: all run benchmark clean

all:
	$(MAKE) -C pixelsprint all

run:
	$(MAKE) -C pixelsprint run

benchmark:
	$(MAKE) -C pixelsprint benchmark

clean:
	$(MAKE) -C pixelsprint clean
