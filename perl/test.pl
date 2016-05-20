#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

my $host = $ARGV[0] || '127.0.0.1';

my $dali = usbdali->new($host);
if ($dali->connect()) {
	$dali->send(usbdali->make_cmd('broadcast', 'off'));
	my $resp = $dali->receive();
	if ($resp) {
		print("Received status:$resp->{status}\n");
	} else {
		print("Receive error\n");
	}
	$dali->disconnect();
} else {
	print("Can't connect\n");
}
