set terminal png
set datafile separator ","

set title "G_1: Throughput vs Threads# for Mutex and Spin-lock"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_1.png'
plot \
     "< grep 'list-none-m,[0-9]*,1000,1' lab2b_list.csv" using ($2):(1000000000/($7)) title 'Mutex list op' with linespoints lc rgb 'red', \
     "< grep 'list-none-s,[0-9]*,1000,1' lab2b_list.csv" using ($2):(1000000000/($7)) title 'Spin-lock list op' with linespoints lc rgb 'green'
