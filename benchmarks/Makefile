CC = mpicc
CFLAGS = -DFLOAT_PRECISION=2 -DFIELD_WIDTH=18

TARGETS = comm_creation_overhead osu_mbw_mr osu_mbw_mr_async halo

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -rf $(TARGETS) *.dSYM
