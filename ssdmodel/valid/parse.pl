#!/usr/bin/perl

use strict;
use warnings;

$" = " ";

my @key = ("all_response_time", "ssd_response_time", "all_worst_response_time", 
		"ssd_worst_response_time", "host_read_num", "host_write_num", "gc_num", 
		"gc_read_num", "acc_light_loading_time per req", "acc_light_loading_time prob", 
		"total_complete_req", "simtime", "portion");
my @result = undef;

open FH, $ARGV[0];

while(<FH>) {

	if(/\=\=\W(.*?)\W\=\=/) {
		chomp @result;
		print "@result\n" if ($. != 1);
		print "$1 ";
	}

	my $line = $_;

	for(my $i = 0; $i < scalar @key; $i ++) {
		if($line =~ /^$key[$i]/) {
			$line =~ m/\:(.*?)$/;
			my $value = $1;
			$result[$i] = $value;
			last;
		}
	}
}

print "@result\n";

# seek FH, 0, 0;
# 
# @key = ("acc_read_response_time_per_process", "acc_write_response_time_per_process", "read_request_per_process", "write_request_per_process", "ave_response_time_per_process");
# @result = undef;
# 
# while(<FH>) {
# 	if(/\=\=\W(.*?)\W\=\=/) {
# 		chomp @result;
# 		if ($. != 1) {
# 			for(my $i = 0; $i < scalar @key; $i ++) {
# 				$\ = "\n";
# 				print $key[$i], $result[$i];
# 			}
# 		}
# 		print "$1 ";
# 	}
# 
# 	my $line = $_;
# 
# 	for(my $i = 0; $i < scalar @key; $i ++) {
# 		if($line =~ /^$key[$i]/) {
# 			$line =~ m/\:(.*?)$/;
# 			my $value = $1;
# 			$result[$i] = $value;
# 			last;
# 		}
# 	}
# }
# 
# for(my $i = 0; $i < scalar @key; $i ++) {
# 	$\ = "\n";
# 	print $key[$i], $result[$i];
# }
