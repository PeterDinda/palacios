#!/usr/bin/perl -w

use Getopt::Long;

&GetOptions(32=>\$m32, 64=>\$m64, "output=s"=>\$mod);

$#ARGV >= 0 or die "usage: compile-for-static-user-level-injection.pl [-32|-64] [--output=module_name] source.c+ [lib.a]*\n";

if (!$m32 && !$m64) {
  print "Assuming 32 bit.  Use -64 to override\n";
  $m32=1;
}

if (!$mod) { 
  print "No module name given, assuming a.tooth\n";
  $mod = "a.tooth";
}

if ($m32) { 
  $gopt = "-m32";
  $lopt = "-melf_i386 --oformat elf32-i386";
} 

if ($m64) { 
  $gopt = "-m64";
  $lopt = "-melf_x86_64 --oformat elf64-x86-64";
}


$linkerscript = <<END;
SECTIONS
{
/* Must be on a page boundary */
/* Should link like ld -z max-page-size=4096 -T ld.script ... */
/* If object file is -fPIC, then it shouldn't matter where we load it */
  . = 0x1000;   
/* Text, data, and bss squished together */
  .text : { *(.text) }
  .data : { *(.data) }
  .bss : { *(.bss) }
/* Result will be one load group marked RWX */
}
END



@stems=grep(/.*\.c$/,@ARGV);
@libs=grep(/.*\.a$/,@ARGV);

map { $_ =~ s/\.c$//g} @stems;

print "Compiling...\n";
foreach $s (@stems) { 
  system("gcc $gopt -fPIE -Wa,-R -c $s.c -nostartfiles -nodefaultlibs -nostdlib -static -o $s.o") == 0 
   or die "Compilation of $s.c failed\n";
  system("gcc $gopt -fPIE -Wa,-R -S $s.c -nostartfiles -nodefaultlibs -nostdlib -static -o $s.s") == 0 
   or die "Compilation of $s.c failed\n";
}
print "Compilation done.\n";

open(W,">.linker_script");
print W $linkerscript;
close(W);

print "Linking...\n";

$rc=system("ld $lopt -z max-page-size=4096 -T .linker_script ".join(" ",map { "$_.o" } @stems)." ".join(" ",@libs)." -o $mod\n");

unlink ".linker_script";

$rc==0 or die "Linking of $mod failed\n";

print "Linking of $mod completed.  Done.\n";

open(E,"readelf -h $mod |");
while (<E>) { 
  if (/^\s*Entry point address:\s+(\S+)$/) {
    print "Entry point relative to beginning of file: $1\n";
    last;
  }
}
close(E);
  
