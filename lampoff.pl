#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

my $dali = usbdali->new('localhost');
$dali->connect() || die;
$dali->send($dali->make_cmd('lamp', 0, 'off'));
$dali->disconnect();
