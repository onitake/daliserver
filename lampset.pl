#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

if (@ARGV < 2) {
	print("Usage: lampset.pl <0-15> <0-254>\n");
	exit(1);
}

my $lamp = $ARGV[0];
if ($lamp < 0) {
	$lamp = 0;
}
if ($lamp > 15) {
	$lamp = 15;
}
my $dim = $ARGV[1];
if ($dim < 0) {
	$dim = 0;
}
if ($dim > 254) {
	$dim = 254;
}

my $dali = usbdali->new('localhost');
$dali->connect() || die;
$dali->send($dali->make_dim('lamp', $lamp, $level));
$dali->disconnect();
