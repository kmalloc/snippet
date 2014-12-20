gcc -shared func.c -fPIC -o libfunc.so
gcc -shared preload.c -fPIC -o libpreload.so
gcc -o main main.c -L. -lfunc

echo "running main...."
./main

echo "running main with preload...."
LD_PRELOAD=libpreload.so LD_LIBRARRY_PATH=. ./main
