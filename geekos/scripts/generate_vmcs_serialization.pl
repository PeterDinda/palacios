#!/usr/bin/perl

$#ARGV==0 or die "gimme a filename\n";

$file=shift;

@list=();

open(HEADER,">$file.h");
open(SOURCE,">$file.c");

print HEADER "#ifndef $file\n";
print HEADER "#define $file\n";
print HEADER "#include <geekos/vmcs.h>\n";

print SOURCE "#include <geekos/$file.h>\n";

while (<STDIN>) {
  if (/\#define\s+(\S+)\s+/) {
    push @list, $1;
    GenSerUnserCode($1);
  }
}

GenPrintAllCode(@list);

print HEADER "#endif\n";

sub GenSerUnserCode {
  my $name=shift;

  print SOURCE <<END

void    Set_$name(uint_t val) { VMCS_WRITE($name,val); }
uint_t  Get_$name() { uint_t rc; VMCS_READ($name,&rc); return rc; }

void    Print_$name() { PrintTrace("$name = %x\\n", Get_$name()); }

END

;
  print HEADER <<END2

void    Set_$name(uint_t val);
uint_t  Get_$name();

void    Print_$name();

END2

;

}


sub GenPrintAllCode  {
  print SOURCE "void PrintTrace_VMCS_ALL() {\n";
  while (my $name=shift) { 
    print SOURCE "  PrintTrace_$name();\n";
  }
  print SOURCE "}\n";
  print HEADER "void PrintTrace_VMCS_ALL();\n";
}
