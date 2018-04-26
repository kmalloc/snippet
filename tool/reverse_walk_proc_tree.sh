pid=$1
while ((pid!=1));do
    echo "$pid:`readlink /proc/$pid/exe`"
    pid="`cat /proc/$pid/status|grep PPid|grep -v grep|awk 'BEGIN{FS=" "}{print $2}'`"
done
