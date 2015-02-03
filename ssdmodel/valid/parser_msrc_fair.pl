#!/usr/bin/perl

use warnings;
use strict;

my $fn_base = $ARGV[0];

open FH, $fn_base or die("Can't openfile: ".$fn_base);

my $key = "ave_response_time_per_process";
my @workload = ();
my @avg_resp_time = ();

while(<FH>) {

	if(m/== (.*) ==/) {
		push @workload, $1;
	}
	elsif(m/^$key/) {
		m/: (.*)$/;
		my @list = split(/ /, $1);
		push @avg_resp_time, \@list;
	}
}

close FH;

$key= "overall_response_time";
my @overall_resp_time = ();

foreach my $i (0 ... $#workload) {
	my @list = ();
	$overall_resp_time[$i] = \@list;
}

$fn_base="./fair/out_noop_msrc_";

foreach my $i (0 ... 19) {
	open FH, $fn_base.$i;
	my @wkld = ();

	while(<FH>) {
		
		if(m/^$key/) {
			m/: (.*)$/;
			push @wkld, $1;
		}
	}

	foreach my $j (0 ... $#wkld) {
		$overall_resp_time[$j][$i] = $wkld[$j];
	}

	close FH;
}

foreach my $wkld (0 ... $#workload) {
	print $workload[$wkld];
	
	foreach my $core (0 ... 19) {

		printf " %.6f", $avg_resp_time[$wkld][$core]/$overall_resp_time[$wkld][$core];
	}

	print "\n";
}
