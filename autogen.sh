#! /bin/bash

# default MPI path
mpidir=`which mpicc| sed -e 's_/[^/]*$__g'`/../
mpih_path=

# user specified MPI path
for arg in "$@" ; do
    case $arg in
        -with-mpi=*|--with-mpi=*)
            mpidir=`echo "A$arg" | sed -e 's/.*=//'`
        ;;

        --help|-h|-help)
        echo "./autogen.sh --with-mpi=<mpi installation path>"
        exit
        ;;

    esac
done

echo -n "Checking header file mpi.h at $mpidir ..."
_mpih=$mpidir/include/mpi.h
if [ ! -f $_mpih ];then
    echo "not found (error)"
    exit 1
else
    echo "done"
    mpih_path=`cd $(dirname $_mpih) ; pwd ; cd $OLDPWD`/mpi.h
fi

echo "Found $mpih_path"

echo -n "Generating MPI wrappers... "
./src/buildiface --infile $mpih_path --outfile src/mpi_wrap.c
echo "done"

autoreconf -vif
