dir1=${1?"please specify old object directory"}
dir2=${2?"please specify new object directory"}
out=${3?"please specify output directory"}

if [ ! -e $out ]; then
    mkdir -p $out
fi

pushd `pwd`

cd $dir1
for f in *.a; do
    ar -x $f
done

old_count=$(ls -l *.o|wc -l)

popd
pushd `pwd`

cd $dir2
for f in *.a; do
    ar -x $f
done

new_count=$(ls -l *.o|wc -l)
popd

if [ $old_count != $new_count ]; then
    echo "object files not the same:$old_count, $new_count"
fi

for f in $dir1/*.o; do
    n=$(basename "$f")
    n=${n%%.*}
    objdump -d $f > "$out/$n.old.d"
done

for f in $dir2/*.o; do
    n=$(basename "$f")
    n=${n%%.*}
    objdump -d $f > "$out/$n.new.d"
done

cd $out

list=$(ls -l *.d|awk '{print $8}'|cut -d . -f 1|sort|uniq)

rm -f ./res.dat

for f in $list; do
    if [ -f $f.old.d ] && [ -f $f.new.d ]; then
        echo "\n" >> ./res.dat
        diff $f.old.d $f.new.d >> ./res.dat
    else
        echo "single file $f"
    fi
done


