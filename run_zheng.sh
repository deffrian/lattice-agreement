#usage num_of_proc my_ip coordinator_ip 

for (( i=1; i <= $1; i++))
do
	sleep 0.1
        ./build/zheng/zheng $2 $((8000+$i)) 8000 $3 $((8200+$i)) > $i.log &
done

