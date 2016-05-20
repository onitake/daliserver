#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

my $lamp = $ARGV[0] || 0;
my $dali = usbdali->new('localhost');
if ($dali->connect()) {
	$dali->send($dali->make_cmd('lamp', $lamp, 'off'));
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
