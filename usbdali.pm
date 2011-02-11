package usbdali;

use strict;
use diagnostics;
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
	return bless($self, $class);
}

sub connect {
	my ($self) = @_;
	if (!defined($self->{host})) {
		warn("Can't connect. No hostname set.");
	} else {
		if ($self->{socket}) {
			$self->{socket}->close();
		}
		$self->{socket} = IO::Socket->new() || warn("Can't create socket");
		my $proto = getprotobyname('tcp');
		$self->{socket}->socket(PF_INET, SOCK_STREAM, $proto) || warn("Can't create socket");
		my $address = sockaddr_in($self->{port}, inet_aton($self->{host})) || warn("Invalid host/port");
		$self->{socket}->connect($address) || warn("Can't connect to $self->{host}:$self->{port}");
	}
}

sub disconnect {
	my ($self, $address, $cmd) = @_;
	if ($self->{socket}) {
		$self->{socket}->close();
	}
}

sub send {
	my ($self, $address, $cmd, $data) = @_;
	if (!$self->{socket} && $self->{socket}->connected()) {
		warn("Can't send command. Socket not connected.\n");
	} else {
		if (!defined($data)) {
			$data = '';
		}
		if (length($data) > 61) {
			$data = substr($data, 0, 64);
		}
		my $packet = pack('CCC', $address, $cmd, length($data));
		$packet .= $data;
		print("Writing packet to socket, length is " . length($packet) . "\n");
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
		$self->{socket}->read($packet, 3);
		my ($address, $cmd, $length) = unpack('CCC', $packet);
		$self->{socket}->read($packet, $length);
		return { address => $address, command => $cmd, data => $packet };
	}
}

1;