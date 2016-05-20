#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

if (@ARGV < 1) {
	print("Usage: allset.pl <0-254>\n");
	exit(1);
}

my $dim = $ARGV[0];
if ($dim < 0) {
	$dim = 0;
}
if ($dim > 254) {
	$dim = 254;
}

my $dali = usbdali->new('localhost');
if ($dali->connect()) {
	$dali->send(usbdali->make_dim('broadcast', $dim));
	my $resp = $dali->receive();
	if ($resp) {
		if ($resp->{status} eq 'response') {
			print("Received status:$resp->{status} response:$resp->{response}\n");
		} else {
			print("Received status:$resp->{status}\n");
		}
	} else {
		print("Receive error\n");
	}
	$dali->disconnect();
} else {
	print("Can't connect\n");
}
