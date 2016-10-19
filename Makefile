.PHONY: cli

LDFLAGS=-llz4 -lboost_iostreams -lz

all: cli test_lz4_filter decompression_test

test: test_lz4_filter
	./test_lz4_filter

cli: lz4fcli

lz4fcli: cli.o lz4_filter.o
	$(CXX) $(LDFLAGS) $+ -o $@

test_lz4_filter: test/test_lz4_filter.o lz4_filter.o
	$(CXX) $(LDFLAGS) $+ -o $@ -l gtest

decompression_test: test/decompression_test.o lz4_filter.o
	$(CXX) $(LDFLAGS) $+ -o $@ -l gtest

clean:
	rm -f *.o test/*.o test_lz4_filter lz4fcli
