#!/usr/bin/perl -w

use Getopt::Long;
use v3_checkpoint_file;
use Data::Dumper;

$skipmem=0;

&GetOptions("skipmem"=>\$skipmem);

if ($skipmem) { 
  @skiplist=("memory_img");
}

$#ARGV==1 or die "v3_checkpoint_copy_ini_dir_to_binary_dir.pl [--skipmem] ini_dir bin_dir\n";

$inidir = shift;
$bindir = shift;

my $hr = v3_read_checkpoint_from_ini_dir($inidir,@skiplist);

v3_write_checkpoint_as_binary_dir($hr,$bindir);

