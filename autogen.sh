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

# generate MPI wrapper functions
wrap_file=src/user/mpi_wrap.c

# - MPI
echo_n "Checking header file mpi.h at $mpidir ..."
mpih_file=$mpidir/include/mpi.h
if [ ! -f $mpih_file ];then
    echo "not found (error)"
    exit 1
else
    echo "done"
    mpih_path=`cd $(dirname $mpih_file) ; pwd ; cd $OLDPWD`/mpi.h
    echo "Found $mpih_path"
fi

echo_n "Generating MPI wrappers... "
./maint/buildiface --infile $mpih_path --outfile $wrap_file
echo "done"

# - MPI IO
echo ""
echo_n "Checking header file mpio.h at $mpidir ..."
mpioh_file=$mpidir/include/mpio.h
if [ ! -f $mpioh_file ];then
    echo "not found"
else
    echo "done"
    mpioh_path=`cd $(dirname $mpioh_file) ; pwd ; cd $OLDPWD`/mpio.h
    echo "Found $mpioh_path"

    # add IO functions only when MPI supports it
		echo_n "Generating MPI IO wrappers... "
		./maint/buildiface --infile $mpioh_path --outfile $wrap_file --append
		echo "done"
fi

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
