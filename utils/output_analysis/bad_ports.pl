#!/usr/bin/perl -w

$#ARGV==0 or die "Finds all unique unhandled I/O ports in a palacios output file\nusage: bad_ports.pl serial.out\n";

open(K,shift);

while (<K>) { 
  if (/: (\S+) operation on unhooked IO port 0x(\S+)/) {
    $dir=$1;
    $port=$2;

    $p{$port} |= ($dir eq 'IN' ? 1 : 2);
    $n{$port}++;
  }
}

close(K);

@list = sort keys %p;

foreach $port (@list) { 
  print $port,"\t",$n{$port};
  if ($p{$port} & 1) { 
    print "\tIN";
  }
  if ($p{$port} & 2) { 
    print "\tOUT";
  }
  print "\n";
}
    
