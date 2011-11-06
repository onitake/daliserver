# Copyright (c) 2011, onitake <onitake@gmail.com>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#    1. Redistributions of source code must retain the above copyright notice, this list of
#       conditions and the following disclaimer.
# 
#    2. Redistributions in binary form must reproduce the above copyright notice, this list
#       of conditions and the following disclaimer in the documentation and/or other materials
#       provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package usbdali;

use strict;
use diagnostics;
use feature 'switch';
use IO::Socket;
use Socket;

sub new {
	my $class = shift();
	my ($host) = @_;
	my $self = { };
	if ($host && !ref($host)) {
		$self->{host} = $host;
	}
	$self->{port} = 55825;
	$self->{slaves} = { };
	return bless($self, $class);
}

sub connect {
	my ($self) = @_;
	if (!defined($self->{host})) {
		warn("Can't connect. No hostname set.");
		return undef;
	} else {
		if ($self->{socket}) {
			$self->{socket}->close();
		}
		$self->{socket} = IO::Socket->new();
		if (!$self->{socket}) {
			warn("Can't create socket");
			return undef;
		}
		my $proto = getprotobyname('tcp');
		if (!$self->{socket}->socket(PF_INET, SOCK_STREAM, $proto)) {
			warn("Can't create socket");
			return undef;
		}
		my $address = sockaddr_in($self->{port}, inet_aton($self->{host}));
		if (!$address) {
			warn("Invalid host/port");
			return undef;
		}
		if (!$self->{socket}->connect($address)) {
			warn("Can't connect to $self->{host}:$self->{port}");
			return undef;
		}
	}
	return 1;
}

sub disconnect {
	my ($self, $address, $cmd) = @_;
	if ($self->{socket}) {
		$self->{socket}->close();
	}
}

sub send {
	my ($self, $address, $cmd) = @_;
	if (!$self->{socket} && $self->{socket}->connected()) {
		warn("Can't send command. Socket not connected.\n");
	} else {
		my $packet = pack('CC', $address, $cmd);
		print("Writing " . length($packet) . " bytes to socket, address=$address command=$cmd\n");
		my $socket = $self->{socket};
		print($socket $packet);
	}
}

sub receive {
	my ($self) = @_;
	if (!$self->{socket} && $self->{socket}->connected()) {
		warn("Can't receive command. Socket not connected.\n");
	} else {
		my $packet;
		$self->{socket}->read($packet, 2);
		my ($status, $response) = unpack('CC', $packet);
		my $ret = { status => $status, response => $response };
		given ($status) {
			when (0) {
				$ret->{status} = 'ok';
			}
			when (1) {
				$ret->{status} = 'error';
			}
		}
		return $ret;
	}
}

sub scan_devices {
	my ($self) = @_;
}

sub add_device {
	my ($self, $index) = @_;
	$self->{devices}->[$index] = {
		current_level => 0,
		poweron_level => 0,
		failure_level => 0,
		min_level => 0,
		max_level => 0,
		fade_rate => 0,
		fade_time => 0,
		short_address => 0,
		search_address => 0,
		random_address => 0,
		group => [ ], # 16 bytes
		scene => [ ], # 16 bytes
		status => 0,
		version => 0,
		physical_min => 0,
	};
}

sub make_dim {
	my ($self, $type, $dest, $value) = @_;
	my @ret;
	given ($type) {
		when (/lamp/) {
			@ret = (($dest & 0x3f) << 1, $value);
		}
		when (/group/) {
			@ret = (0x80 | (($dest & 0xf) << 1), $value);
		}
		when (/broadcast/) {
			@ret = (0xfe, $dest);
		}
		default {
			@ret = (0, 0);
		}
	}
	return @ret;
}

sub make_cmd {
	my ($self, $type, $dest, $cmd, $arg) = @_;
	my $address;
	given ($type) {
		when (/lamp/) {
			$address = 0x1 | (($dest & 0x3f) << 1);
		}
		when (/group/) {
			$address = 0x81 | (($dest & 0xf) << 1);
		}
		when (/broadcast/) {
			$address = 0xff;
			$arg = $cmd;
			$cmd = $dest;
		}
		when (/special/) {
			my $code0;
			my $code1;
			given ($dest) {
				when (/term/) {
					$code0 = 0xa1;
					$code1 = 0x00;
				}
				when (/dtr/) {
					$code0 = 0xa3;
					$code1 = $cmd & 0xff;
				}
				when (/init/) {
					$code0 = 0xa5;
					$code1 = $cmd & 0xff;
				}
				when (/random/) {
					$code0 = 0xa7;
					$code1 = 0x00;
				}
				when (/compare/) {
					$code0 = 0xa9;
					$code1 = 0x00;
				}
				when (/withdraw/) {
					$code0 = 0xab;
					$code1 = 0x00;
				}
				when (/searchh/) {
					$code0 = 0xb1;
					$code1 = $cmd & 0xff;
				}
				when (/searchm/) {
					$code0 = 0xb3;
					$code1 = $cmd & 0xff;
				}
				when (/searchl/) {
					$code0 = 0xb5;
					$code1 = $cmd & 0xff;
				}
				when (/set/) {
					$code0 = 0xb7;
					$code1 = $cmd & 0xff;
				}
				when (/check/) {
					$code0 = 0xb9;
					$code1 = $cmd & 0xff;
				}
				when (/address/) {
					$code0 = 0xbb;
					$code1 = 0x00;
				}
				when (/phys/) {
					$code0 = 0xbd;
					$code1 = 0x00;
				}
				default {
					$code0 = 0x00;
					$code1 = 0x00;
				}
			}
			return ($code0, $code1);
		}
		default {
			$address = 0x00;
		}
	}
	my $code;
	given ($cmd) {
		when (/off/) {
			$code = 0x00;
		}
		when (/dim.?up/) {
			$code = 0x01;
		}
		when (/dim.?down/) {
			$code = 0x02;
		}
		when (/inc/) {
			$code = 0x03;
		}
		when (/dec/) {
			$code = 0x04;
		}
		when (/set.?max/) {
			$code = 0x05;
		}
		when (/set.?min/) {
			$code = 0x06;
		}
		when (/down/) {
			# duplicate of command 4
			$code = 0x07;
		}
		when (/up/) {
			# duplicate of command 3
			$code = 0x08;
		}
		when (/set.?scene/) {
			$code = 0x10 | ($arg | 0xf);
		}
		when (/reset/) {
			$code = 0x20;
		}
		when (/store.?dtr/) {
			$code = 0x21;
		}
		when (/store.?max/) {
			$code = 0x2a;
		}
		when (/store.?min/) {
			$code = 0x2b;
		}
		when (/store.?fail/) {
			$code = 0x2c;
		}
		when (/store.?power/) {
			$code = 0x2d;
		}
		when (/store.*time/) {
			$code = 0x2e;
		}
		when (/store.*rate/) {
			$code = 0x2f;
		}
		when (/store.?scene/) {
			$code = 0x40 | ($arg | 0xf);
		}
		when (/remove.?scene/) {
			$code = 0x50 | ($arg | 0xf);
		}
		when (/add.?group/) {
			$code = 0x60 | ($arg | 0xf);
		}
		when (/remove.?group/) {
			$code = 0x70 | ($arg | 0xf);
		}
		when (/store.?address/) {
			$code = 0x80;
		}
		when (/status/) {
			$code = 0x90;
		}
		when (/check.?work/) {
			$code = 0x91;
		}
		when (/check.?lamp/) {
			$code = 0x92;
		}
		when (/check.?operat/) {
			$code = 0x93;
		}
		when (/check.?level/) {
			$code = 0x94;
		}
		when (/check.?reset/) {
			$code = 0x95;
		}
		when (/check.?address/) {
			$code = 0x96;
		}
		when (/version/) {
			$code = 0x97;
		}
		when (/dtr/) {
			$code = 0x98;
		}
		when (/type/) {
			$code = 0x99;
		}
		when (/physical/) {
			$code = 0x9a;
		}
		when (/check.?fail/) {
			$code = 0x9b;
		}
		when (/level/) {
			$code = 0xa0;
		}
		when (/max/) {
			$code = 0xa1;
		}
		when (/min/) {
			$code = 0xa2;
		}
		when (/power/) {
			$code = 0xa3;
		}
		when (/fail/) {
			$code = 0xa4;
		}
		when (/(time)|(rate)/) {
			$code = 0xa5;
		}
		when (/scene/) {
			$code = 0xb0 | ($arg | 0xf);
		}
		when (/group0/) {
			$code = 0xc0;
		}
		when (/group8/) {
			$code = 0xc1;
		}
		when (/randomh/) {
			$code = 0xc2;
		}
		when (/randomm/) {
			$code = 0xc3;
		}
		when (/randoml/) {
			$code = 0xc4;
		}
		default {
			$code = 0x00;
		}
	}
	return ($address, $code);
}

1;
