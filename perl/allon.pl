#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

my $dali = usbdali->new('localhost');
if ($dali->connect()) {
	$dali->send(usbdali->make_cmd('broadcast', 'up'));
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
