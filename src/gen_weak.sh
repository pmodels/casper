if [ $# != 1 ]; then
	echo "./gen_weak.sh <filename>"
	exit
fi

ffile=tmp
wfile=pragma_weak
srcfile=$1.in
distfile=$1

echo "gen $distfile..."

cat $srcfile |grep "#define FCNAME MPIASP_" > $ffile
sed -i 's/#define FCNAME //' $ffile

cat <<EOF > $wfile
/* -- Begin Profiling Symbol Block for routine MPI_Init */
#if defined(HAVE_PRAGMA_WEAK) 
EOF

for func in `cat $ffile`
do
	func=`echo $func|tr -d "\r\n"`
	orgfunc=${func/MPIASP_/MPI_}
	echo "#pragma weak $orgfunc = $func" >> $wfile
done

cat <<EOF >> $wfile

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
EOF

for func in `cat $ffile`
do
	func=`echo $func|tr -d "\r\n"`
	orgfunc=${func/MPIASP_/MPI_}
	echo "#pragma _HP_SECONDARY_DEF $func $orgfunc" >> $wfile
done

cat <<EOF >> $wfile

#elif defined(HAVE_PRAGMA_CRI_DUP)
EOF

for func in `cat $ffile`
do
	func=`echo $func|tr -d "\r\n"`
	orgfunc=${func/MPIASP_/MPI_}
	echo "#pragma _CRI duplicate $orgfunc as $func" >> $wfile
done

cat <<EOF >> $wfile
#endif
/* -- End Profiling Symbol Block */
EOF

unix2dos $wfile

cp $srcfile $distfile
sed -i "/@PRAGMA_WEAK@/r ${wfile}" $distfile
sed -i 's/@PRAGMA_WEAK@//' $distfile

rm $ffile
rm $wfile
