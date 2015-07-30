#! /bin/bash

# default MPI path
# drop any error reported by which command.
tmp_mpidir=`which mpicc 2>/dev/null`
mpidir=`echo $tmp_mpidir| sed -e 's_/[^/]*$__g'`/../
mpih_path=

echo_n() {
	# "echo_n" isn't portable, must portably implement with printf
	printf "%s" "$*"
}

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

echo_n "Checking header file mpi.h at $mpidir ..."
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
echo_n "Generating MPI wrappers... "
./src/buildiface --infile $mpih_path --outfile src/mpi_wrap.c
echo "done"

subdirs=test

# copy confdb
echo ""
for subdir in $subdirs ; do
	subconfdb_dir=$subdir/confdb
	echo_n "Syncronizing confdb -> $subconfdb_dir... "
	if [ -x $subconfdb_dir ] ; then
		rm -rf "$subconfdb_dir"
	fi
	cp -pPR confdb "$subconfdb_dir"
	echo "done"
done

# generate configures
for subdir in . $subdirs ; do
	echo ""
	echo "Generating configure in $subdir"
	(cd $subdir && autoreconf -vif) || exit 1
done
