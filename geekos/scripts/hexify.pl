#!/usr/bin/perl

$file = $ARGV[0];

while(1) { 
	open (FILE,">>$file");
	$foo = <STDIN>; 
	chomp $foo; 
	print FILE pack("C", hex("0x$foo")); 
	close FILE;
}

