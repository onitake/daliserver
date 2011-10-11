#!/usr/bin/perl

use strict;
use warnings;
use usbdali;
use Data::Dumper;

my $dali = usbdali->new('localhost');
$dali->connect();
$dali->send($dali->make_cmd('broadcast', 'off'));
$dali->disconnect();
