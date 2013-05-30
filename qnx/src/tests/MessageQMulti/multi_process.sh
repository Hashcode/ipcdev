# Run MessageQMulti in parallel processes
#
if [ $# -ne 2 ]
then
    echo "Usage: multi_process <num_processes> <iterations_per_process>"
    exit
fi

num=0; while [ $num -lt $1 ];  do
    ((num = $num + 1))
	echo "MessageQMulti Test #" $num
	# This calls MessageQMulti with One Thread, a process #, and
	# number of iterations per thread.
	MessageQMulti 1 $2 $num &
done
