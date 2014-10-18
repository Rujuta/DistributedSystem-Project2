#!/bin/bash

usage()
{
	echo "Usage: -p <No of packets to send> -n <No of machines to use> -l <Loss to use> -h <Usage>"
	exit 1;
}	
PROCESS_NAME="./mcast"
HOME_DIR="/home/rdeshpa3/DistributedSystems/Projects/Project2/DistributedSystem-Project2"
HOST_LIST="ugrad13 ugrad14 ugrad15 ugrad16 ugrad17 ugrad18 ugrad19 ugrad20 ugrad21 ugrad22 ugrad23 ugrad24"

# No of parameters < 4 display usage
#if [ "$#" -ne 4 ]
#then
#	usage
#fi

while getopts ":p:n:l:h:" OPT; do
	case "${OPT}" in 
	p) 	
		PACKETS=${OPTARG}
		echo $PACKETS ;;
	n) 	MACHINES=${OPTARG} 
		echo $MACHINES;;
	l) 	LOSS=${OPTARG}
		echo $LOSS;;
	h) 	usage ;;
	esac
done

cd $HOME_DIR
make clean
make
id=0
for machine in $HOST_LIST
do
	if [[ "$id" -eq "$MACHINES" ]]
	then
		echo "Done starting mcast everywhere"
		exit 0
	fi
	ssh -f $machine "cd ${HOME_DIR} ; nohup $PROCESS_NAME $PACKETS $id $MACHINES $LOSS > /dev/null & "  
#$PROCESS_NAME $PACKETS $id $MACHINES $LOSS
	if [ "$?" -ne 0 ]; then echo "Failed to start $id" ; exit 1; fi
	echo "Started process $id"
	(( id++ ))
	echo $id
	
done


#while read HOST
#do
#   ssh amehta26@"$HOST" "hostname; ls;" < /dev/null
#done < "line.txt"
