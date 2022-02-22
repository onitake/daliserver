#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

if (@ARGV < 2) {
	print("Usage: lamptemp.pl <0-63> <1000-20000>\n");
	exit(1);
}

my $lamp = $ARGV[0];
my $temp = $ARGV[1];
if ($lamp < 0) {
	$lamp = 0;
}
if ($lamp > 63) {
	$lamp = 63;
}
if ($temp < 1000) {
	$temp = 0;
}
if ($temp > 20000) {
	$temp = 20000;
}

my $mirek = int(1000000 / $temp);
my $mirek_lsb = $mirek & 0xff;
my $mirek_msb = ($mirek >> 8) & 0xff;
print("Setting color temperature to $temp K ($mirek Mirek)\n");

my $dali = usbdali->new('localhost');
if ($dali->connect()) {
	$dali->send(usbdali->make_cmd('special', 'dtr0', $mirek_lsb));
	$dali->send(usbdali->make_cmd('special', 'dtr1', $mirek_msb));
	$dali->send(usbdali->make_cmd('special', 'type', 8));
	$dali->send(usbdali->make_cmd('lamp', $lamp, 'temperature'));
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
