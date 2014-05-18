boost_lz4_filter
================

lz4_compressor &amp; lz4_decompressor boost filters.

Produced data is binary compatible with lz4c utility from https://code.google.com/p/lz4/ programs package.

compiling
---------

1. install boost
2. install lz4 libs/headers ( Gentoo: `emerge app-arch/lz4` :)
3. this will build tests and `lz4fcli` sample commandline utility:
```
  git clone https://github.com/zed-0xff/boost_lz4_filter.git
  cd boost_lz4_filter
  make
```

testing
-------
```
make test
```

using in your own projects
--------------------------
1. install boost
2. install lz4 libs/headers
3. copy `lz4_filter.cpp` & `lz4_filter.hpp` into your own project
4. add `-llz4 -lboost_iostreams` to your compile options
