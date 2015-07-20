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

echo ""
echo -n "Generating MPI wrappers... "
./src/buildiface --infile $mpih_path --outfile src/mpi_wrap.c
echo "done"

subdirs=test

# copy confdb
echo ""
for subdir in $subdirs ; do
	subconfdb_dir=$subdir/confdb
	echo -n "Syncronizing confdb -> $subconfdb_dir... "
	if [ -x $subconfdb_dir ] ; then
		rm -rf "$subconfdb_dir"
		cp -pPR confdb "$subconfdb_dir"
	fi
	echo "done"
done

# generate configures
for subdir in . $subdirs ; do
	echo ""
	echo "Generating configure in $subdir"
	(cd $subdir && autoreconf -vif) || exit 1
done
