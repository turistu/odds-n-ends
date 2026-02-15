#! /usr/bin/perl

# see https://dbus.freedesktop.org/doc/dbus-specification.html 

# this script, while scary-looking, is conceptually banal: it just connects
# to a socket, "authenticates" onto it by writing some stupid fixed strings,
# waits for an OK in reply, and finally packs up and writes a dbus message,
# without bothering to read the reply/ies; all the apparent complexity is
# because of the horrible design of the dbus protocol.

use strict;
use IO::Socket::UNIX;
use IO::Socket::IP;

my ($peer, @cmd) = @ARGV ? @ARGV : qw(/run/systemd/private date);
my $sock = ($peer =~ m{/} ? 'IO::Socket::UNIX' : 'IO::Socket::IP')->new($peer)
	or die "connect: $peer: $!";
$/ = "\r\n";
syswrite $sock, join $/, "\0AUTH EXTERNAL", "DATA", "BEGIN", "";
while(1){ die "unexpected EOF" unless defined ($_ = <$sock>); last if /^OK / }
@cmd = ('/bin/sh', 'sh', '-c', @cmd) if @cmd == 1;
syswrite $sock, pack_dbus(q{
	&y 108 1 4 1
	&u ?len 1
	&a(yv) {
		&r  &y 1  &vo /org/freedesktop/systemd1
		&r  &y 3  &vs StartTransientUnit
		&r  &y 2  &vs org.freedesktop.systemd1.Manager
		&r  &y 6  &vs org.freedesktop.systemd1
		&r  &y 8  &vg ssa(sv)a(sa(sv))
	}
	&r
    &t=.
	&s ["run-".int(rand 1<<32).".service"]
	&s fail
	&a(sv) {
		&r &s ExecStart
		   &va(sasb) {
			&r &s [[ $cmd[0] ]]
			   &as { &s [[ @cmd[1..$#cmd] ]] }
			   &b 0
		   }
	}
	&a(sa(sv))
    &len=.-t
});

package packer {
	sub TIEHASH { my $p = shift; bless {'', \$_[0]}, $p }
	sub FETCH { $_[0]{$_[1]}{v} }
	sub STORE {
		my $v = $_[0]{$_[1]}{v} = $_[2]; my $h = $_[0]{$_[1]};
		substr ${$_[0]{''}}, $$h{o}, $$h{l}, pack $$h{s}, $v if $$h{l};
		$v
	}
	sub pack {
		my $d = shift->{''}; my $s = shift; $$d = pack "a* $s", $$d, @_
	}
}
my (%ts, $ts); BEGIN {
	sub{while(@_){
		$ts{$_}=["x!$_[1]", $_[2]] for split '', $_[0]; splice @_, 0, 3
	}}->(qw[
		y    1 C
		ubh  4 L
		so   4 L/a*x
		g    1 C/a*x
		re({ 8 a0
		a    4 a0
		i 4 l  n 2 s  q 2 S  x 8 q  t 8 Q  d 8 d
	]);
	$ts = qr/[@{[keys %ts]}]/;
}
sub pack_dbus {
	@_ = map /(?{pos})\[+(?{(pos)-$^R}).*?(?:(??{"\\]"x$^R})|$)|\S+/gs, @_;
	my $p = tie my %h, packer => my $d;
	my @o;
	while(@_){
		if(($_ = shift) eq '}'){
			my $o = pop @o or die "unbalanced '}'";
			substr $d, $$o[0], 4, pack 'L', length($d) - $$o[1];
			next
		}
		die "unexpected token '$_'" unless s/^&//;
		if(/=/){
			s/([a-z]\w*)/(\$h{$1})/gi, s/\./(length\$d)/g;
			defined eval or die "$_: $@";
			next
		}
		my $vt = s/^v&?// ? 'C/a*x' : 'a0';
		if(/^a($ts)/){
			$p->pack("$vt x!4", $_); my $o = length $d;
			$p->pack("L $ts{$1}[0]");
			shift, push @o, [$o, length $d] if $_[0] eq '{';
			next;
		}
		$p->pack("$vt x!8", $_), next if $_ eq 'r';
		die "no such type $_" unless /^$ts$/;
		my ($t, @t) = ($_, @{$ts{$_}});
		while(@_ and $_[0] !~ /^[&}]/){
			if(($_ = shift) =~ s/^\?//){
				$p->pack("$vt $t[0]", $t); my $o = length $d;
				$p->pack("x[$t[1]]");
				$$p{$_} = { s => $t[1], o => $o,
						l => length($d) - $o }
			}elsif(/^\[/){
				s/^\[+|\]+$//g;
				defined(my @v = eval) or die "$_: $@";
				$p->pack("$vt @t", $t, $_) for @v;
			}else{
				$p->pack("$vt @t", $t, $_);
			}
		}
	}
	$d
}
