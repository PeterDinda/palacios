#!/usr/bin/perl

$magic = 0xf1e2d3c4;

use FileHandle;

if (scalar(@ARGV) != 2) {
  print STDERR "usage: make_payload.pl <cfg-file> <out-file>\n";
  exit 1;
}

my $config_file = shift @ARGV;
my $out_file = shift @ARGV;

open (CFGFILE, "$config_file");
@cfg = <CFGFILE>;
close CFGFILE;

my $num_regions = 0;

my @region_names = ();
my %region_map = {};

foreach $line (@cfg) {
  chomp $line;
  ($file, $dst) = split(/:/, $line);
  push @region_names, $file;
  $region_map{$file} = hex($dst); #unpack('N', pack("h8",$dst));
  print "" . hex($dst) . "\n";
  $num_regions++;
}



my $fh = new FileHandle(">$out_file");
binmode $fh;

syswrite $fh, pack('L', $magic), 4;
syswrite $fh, pack('L', $num_regions), 4;

foreach $file (@region_names) {
  my $size = (-s $file);

  print "$file to " .  $region_map{$file}. " ($size bytes)\n";
  syswrite $fh, pack('L', $size), 4;
  syswrite $fh, pack('L', $region_map{$file}), 4;
}


my $file;
while (($file = shift @region_names)) {
  my $in_fh = new FileHandle("<$file");
  (defined $in_fh) || die "Couldn't open $file: $!\n";
  binmode $in_fh;

  my $buf = chr(0) x 1024;
  my $n;
  while (($n = sysread($in_fh, $buf, 1024)) > 0) {
    syswrite($fh, $buf, $n);
  }
  $in_fh->close();
}


$fh->close();
