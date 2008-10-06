#!/usr/bin/perl


$file = $ARGV[0];
$ofile = $ARGV[1];

open(INFILE, "$file");
@lines = <INFILE>;
close INFILE;

open(OUTFILE, ">$ofile");


print OUTFILE "\%ifndef VMCS_FIELDS_ASM\n\%define VMCS_FIELDS_ASM\n\n";

foreach $line (@lines) {

  if ($line =~ /\#define\s+(\S+)\s+(\S+)*/) {
    print OUTFILE $1 . " equ " . $2 . "\n";
  }

}


print OUTFILE "\n\%endif\n\n";

close OUTFILE;


