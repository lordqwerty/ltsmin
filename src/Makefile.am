
# LIB DEPENDENCIES (transitive depends on relations)
#
# vset-lib  ---->  bignum-lib
#    |
#    |
#    v
#   HRE   <------>   HRE-IO
#    ^
#    |
#    v
# util-lib
#    ^
#    |
#    |
# ltsmin-lib  <--->  lts-io  <----  lts-lib 
#    ^   ^
#    |    \
#    |     \
#    |      \
#    |      mc-lib
#    |            \
#    |             v
# pins-lib  ---->  dm-lib
#    
#


# dependency matrix lib
SUBDIRS  = dm
# bignum lib
SUBDIRS += bignum

# hybrid runtime environment lib
SUBDIRS += hre-io
SUBDIRS += util-lib
SUBDIRS += hre
if HAVE_MPI
SUBDIRS += hre-mpi
endif

SUBDIRS += gcf-tool

# LTSmin lib with LTS data types and parsers for property languages
SUBDIRS += ltsmin-lib

SUBDIRS += etf-convert

SUBDIRS += lts-io
SUBDIRS += lts-lib
SUBDIRS += ltsmin-printtrace
SUBDIRS += ltsmin-compare
SUBDIRS += ltsmin-convert
SUBDIRS += ltsmin-reduce
if HAVE_MPI
SUBDIRS += ltsmin-reduce-dist
SUBDIRS += ce
endif

# PINS lib, defining the partitioned next-state interface
SUBDIRS += pins-lib

if HAVE_CADP
SUBDIRS += pins-open
endif
SUBDIRS += pins2lts-dist
SUBDIRS += torx

# vset (BDD/MDD) lib
SUBDIRS += vset-lib

# symbolic parity game lib
SUBDIRS += spg-lib

SUBDIRS += spg

if HAVE_SYLVAN
SUBDIRS += pins2lts-sym
endif

if HAVE_ATOMICS
# multi-core lib, lockless data structures and a load balancer
SUBDIRS += mc-lib

SUBDIRS += pins2lts-mc
SUBDIRS += pins2lts-seq
endif

SUBDIRS += scripts
SUBDIRS += tests

# make
.DELETE_ON_ERROR:
