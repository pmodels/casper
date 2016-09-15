#! /bin/sh

# Exit on error
set -ev

os=`uname`
TRAVIS_ROOT="$1"
MPI_IMPL="$2"

# Environment variables
case "$os" in
    Darwin)
        ;;
    Linux)
       export PATH=$TRAVIS_ROOT/mpich/bin:$PATH
       export PATH=$TRAVIS_ROOT/open-mpi/bin:$PATH
       ;;
esac

# Capture details of build
case "$MPI_IMPL" in
    mpich)
        #mpichversion
        mpicc -show
        ;;
    openmpi)
        # this is missing with Mac build it seems
        #ompi_info --arch --config
        mpicc --showme:command
        ;;
esac

# Configure and build
./autogen.sh
./configure CC=mpicc CFLAGS="-std=c99" --disable-static --prefix=/tmp
make V=1
make V=1 install

export CSP_VERBOSE=1

# Run unit tests
make V=1 check

# Run real tests
cd test
make V=1
make V=1 MPIEXEC="mpirun -host localhost " testing
