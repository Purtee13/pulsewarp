# PulseWarp - parallel pairwise DTW on ECG heartbeats (CSC 548)
# On ARC:  module load gcc   (any GCC with OpenMP)  then  make

CC      := gcc
CFLAGS  ?= -O3 -march=native -std=c11 -Wall -Wextra -fopenmp
LDFLAGS ?= -fopenmp -lm

SRC  = src/data.c src/dtw.c src/allpairs.c
OBJ  = $(SRC:src/%.c=build/%.o)
BIN  = pulsewarp
TEST = build/test_dtw

# experiment defaults (override: make golden N=500 DATA=data/mitbih_test.csv)
DATA    ?= data/mitbih_train.csv
N       ?= 300
LENGTHS ?= 64 128 256 512 1024

.PHONY: all test golden verify clean

all: $(BIN)

build:
	mkdir -p build

build/%.o: src/%.c src/pulsewarp.h | build
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ) build/main.o
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST): tests/test_dtw.c $(OBJ) | build
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# unit tests: hand-checked values + every variant bit-identical to serial
test: $(TEST)
	./$(TEST)

# serial all-pairs at every length -> golden reference vectors
golden: $(BIN)
	mkdir -p results/golden
	@for L in $(LENGTHS); do \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy serial \
	    --out results/golden/golden_N$(N)_L$$L.bin --header ; \
	done | tee results/golden/serial_timing_N$(N).csv

# quick correctness check of every strategy against the golden files
verify: $(BIN)
	@for L in $(LENGTHS); do G=results/golden/golden_N$(N)_L$$L.bin; \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy A --schedule static  --threads 4 --golden $$G; \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy A --schedule dynamic --threads 4 --golden $$G; \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy Adiag --schedule dynamic --threads 4 --golden $$G; \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy Bdiag                          --golden $$G; \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy B      --threads 4             --golden $$G; \
	  ./$(BIN) --input $(DATA) --n $(N) --len $$L --strategy Btiled --threads 4             --golden $$G; \
	done

clean:
	rm -rf build $(BIN)
