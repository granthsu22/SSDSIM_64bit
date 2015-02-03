#!/bin/bash

# read arguments
USAGE="Usage: `basename $0` [-t trace_path] [-n parv_filename] [-p parv_filepath] [-s disksim_path] [-o outv_filepath] [-d disksim_name]"
#EXAMPLE="Example: ./`basename $0` -t ~/../../r01/wendy/ssd_trace/MSRC-io-traces-ascii/pid/ -n ssd-1024G_pagesize16.parv -p . -s ../../src -o ~/FIOS_exp_0722/ -d disksim"
EXAMPLE="Example: ./`basename $0` -t ~/../../r01/wendy/ssd_trace/MSRC-io-traces-ascii/pid/ -n ssd-1024G_pagesize16.parv -p . -s ../../src -o . -d disksim"
if [ $# -lt 10 ]; then
	echo $USAGE
	echo $EXAMPLE
	exit 1
fi

while getopts p:s:t:n:o:d: OPTION; do
	case $OPTION in
	n)	parv_filename=$OPTARG
		;;
	p)	parv_filepath=$OPTARG # ~/ssdsim/ssdmodel/valid
		;;
	s)	disksim_path=$OPTARG # ~/ssdsim/src
		;;
	t)	trace_path=$OPTARG # ~/ssd_trace/MSRC-io-traces-ascii
		;;
	o)	outv_filepath=$OPTARG # ~/ssdsim/ssdmodel/valid
		;;
	d)	disksim_name=$OPTARG
		;;
	\?)	echo $USAGE
		echo $EXAMPLE
		exit 1
		;;
	esac
done

if [ -z $parv_filepath ]; then
	parv_filename="./"$parv_filename
else
	parv_filename=$parv_filepath/$parv_filename	
fi

if [ -z $disksim_path ]; then
	PREFIX="../../src"
else
	PREFIX=$disksim_path
fi


if [ ! -e $parv_filename ]; then
	echo "$parv_filename does not exist!"
	exit 1
fi

if [ ! -e $PREFIX/$disksim_name ]; then
	echo "$PREFIX/$disksim_name does not exist!"
	exit 1
fi

if [ ! -d $trace_path ]; then
	echo "$trace_path is not a valid path!"
	exit 1
fi


# read all traces in the director
files=`ls $trace_path/*.trace`

for i in $files; do
	trace_filename=`basename $i`
	base_filename=`echo $trace_filename | cut -d '.' -f 1`

	echo "== $base_filename ==" #> "/home/r01/wendy/output_wendy/out_msrc_$base_filename"
	$PREFIX/$disksim_name $parv_filename $outv_filepath/$base_filename.outv ascii $i 0 #>> "/home/r01/wendy/output_wendy/out_msrc_$base_filename" &
	grep "ssd Response time average:" $outv_filepath/$base_filename.outv | grep -v "#"
done
