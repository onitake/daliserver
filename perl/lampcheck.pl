#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

if (@ARGV < 1) {
	print("Usage: lampcheck.pl <0-63>\n");
	exit(1);
}

my $lamp = $ARGV[0];
if ($lamp < 0) {
	$lamp = 0;
}
if ($lamp > 63) {
	$lamp = 63;
}

my $dali = usbdali->new('localhost');
if ($dali->connect()) {
	$dali->send(usbdali->make_cmd('lamp', $lamp, 'level'));
	my $resp = $dali->receive();
	if ($resp) {
		print("Received status:$resp->{status} response:$resp->{response}\n");
	} else {
		print("Receive error\n");
	}
	$dali->disconnect();
} else {
	print("Can't connect\n");
}
