#! /bin/bash

path=`which mpicc | sed -e 's_/[^/]*$__g'`/../include
path=`cd $path ; pwd ; cd $OLDPWD`/mpi.h

echo -n "Generating MPI wrappers... "
./src/buildiface --infile $path --outfile src/mpi_wrap.c
echo "done"

autoreconf -vif
