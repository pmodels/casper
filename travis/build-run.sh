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
        # see https://github.com/open-mpi/ompi/issues/2956
        export TMPDIR=/tmp
        ;;
esac

# Configure and build
./autogen.sh
./configure CC=mpicc CFLAGS="-std=c99" --disable-static --prefix=/tmp
make V=1
make V=1 install

# Set test configuration
export CSP_VERBOSE=4
export MPIEXEC_TIMEOUT=600 # in seconds

TEST_MPIEXEC=
case "$MPI_IMPL" in
    mpich)
        TEST_MPIEXEC="mpiexec -np"
        ;;
    openmpi)
        # --oversubscribe fixes error "Either request fewer slots for your 
        # application, or make more slots available for use." on osx.
        # see https://github.com/open-mpi/ompi/issues/3133
        TEST_MPIEXEC="mpiexec --oversubscribe -np"
        ;;
esac

# Run unit tests
export CSP_ASYNC_MODE="rma|pt2pt"
echo "Run unit tests with CSP_ASYNC_MODE=$CSP_ASYNC_MODE"
make check MPIEXEC="$TEST_MPIEXEC" MAX_NP=5
