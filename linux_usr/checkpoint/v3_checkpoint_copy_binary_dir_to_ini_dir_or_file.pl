#!/usr/bin/perl -w

use Getopt::Long;
use v3_checkpoint_file;

$skipmem=0;
$asfile=0;

&GetOptions("skipmem"=>\$skipmem, "asfile"=>\$asfile);

if ($skipmem) { 
  @skiplist=("memory_img");
}

$#ARGV==1 or die "v3_checkpoint_copy_binary_dir_to_ini_dir_or_file.pl [--skipmem] [--asfile] binary_checkpoint_dir ini_dir_or_file\n";

$dir = shift;
$ini = shift;

my $hr = v3_read_checkpoint_from_binary_dir($dir,@skiplist);

if ($asfile) { 
  v3_write_checkpoint_as_ini_file($hr,$ini);
} else {
  v3_write_checkpoint_as_ini_dir($hr,$ini);
}

