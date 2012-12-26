#!/usr/bin/perl -w

use v3_checkpoint_file;
use Data::Dumper;

$#ARGV==1 or die "v3_checkpoint_copy_binary_file_to_ini_file.pl binary_checkpoint_file ini_file\n";

$binfile = shift;
$inifile = shift;

if ($binfile=~/.*\/(.*)$/) { 
  $key=$1;
} else {
  $key=$binfile;
}

my $hr = v3_read_checkpoint_binary_file($binfile);

v3_write_checkpoint_as_ini_file($hr,$inifile,$key);
