#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

my $lamp = $ARGV[0] || 0;
my $dali = usbdali->new('localhost');
$dali->connect() || die;
$dali->send($dali->make_dim('lamp', $lamp, 80));
$dali->disconnect();
