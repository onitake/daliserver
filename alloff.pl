#!/usr/bin/perl

use strict;
use warnings;
use usbdali;

my $dali = usbdali->new('localhost');
$dali->connect();
$dali->send(0xff, 0x00);
$dali->disconnect();
