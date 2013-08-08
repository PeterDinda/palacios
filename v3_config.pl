#!/usr/bin/perl -w


print <<END

Welcome.  The purpose of this tool is to simplify configuration
of the V3VEE environment on a Linux system.   It assumes you
have already built Palacios, the Linux embedding, and the Linux
user-sapce tools.   If you haven't, hit CTRL-C now and do the
at least the following:

  make menuconfig 
     [choose your options]
  make
  cd linux_usr
  make
  cd ..
  ./v3_config.pl


This tool will create (or overwrite) the following for you:

  ./ENV       An environment file that you can source
  ./v3_init   A script that will insert Palacios with
              relevant options and with appropriate memory
              allocations.
  ./v3_deinit A script that will free memory and remove 
              Palacios.

After these are created, you can insert Palacio in the following way:

  source ./ENV
  ./v3_init

Then create and run VMs:

  v3_create image_file name
  v3_launch /dev/v3-vmN
  v3_stop /dev/v3-vmN
  v3_free /dev/v3-vmN

And you can remove Palacios and free memory it is using.

  ./v3_deinit


We begin with a set of questions.

END
;

$pdir = `pwd`; chomp($pdir);

print "What is your Palacios directory? [$pdir] : ";

$pdir = get_user($pdir);

$kdir = "/boot";

print "Where are your Linux kernel config files? [$kdir] : ";

$kdir = get_user($kdir);

$hotremove = get_kernel_feature($kdir, "CONFIG_MEMORY_HOTREMOVE");

if (!defined($hotremove)) { 
  $hotremove="n";
}

$canhotremove=$hotremove;

$memblocksize = get_palacios_core_feature($pdir,"V3_CONFIG_MEM_BLOCK_SIZE");

if (!defined($memblocksize)) { 
  print "Cannot determine your memory block size from your Palacios configuration.\n";
  exit -1;
}

if (!powerof2($memblocksize)) { 
  print "Cannot handle a memory block size that is not a power of two ($memblocksize)...\n";
  exit -1;
}

$compmemblocksize = $memblocksize;

$maxalloc = 4194304;

print "What is your kernel's maximum contiguous page allocation size in bytes (typicaly (MAX_ORDER-1)*4096) [$maxalloc] : ";

$maxalloc = get_user($maxalloc);

$shadow = 'y';

print "Do you need to run guests with shadow paging or for other reasons that require 4GB enforcement of page allocation? [$shadow] : ";

$shadow = get_user($shadow);

if ($hotremove eq "y") {
  print "Your kernel supports hot remove.  Do you want to use it? [$hotremove] : ";
  $hotremove = get_user($hotremove);
}


$override_memblocksize = 'n';

if ($hotremove eq "n") {
  do  { 
     $override_memblocksize = 'y';
     print "You are not using hot-remove, so we will adjust memory block size\n";
     print "Desired memory block size? [$maxalloc or less, power of 2] : ";
     $memblocksize = get_user($maxalloc);
  } while ($memblocksize>$maxalloc && !powerof2($memblocksize));
}

$mem = 1024;

print "How much memory (in MB) do you want to initially allocate for Palacios? [$mem] : ";
$mem = get_user($mem);

$devmem='y';

print "Do you need userspace access to your VMs' physical memory? [$devmem] : ";
$devmem = get_user($devmem);

print <<END2

Parameters
   Palacios Direcotry:          $pdir
   Kernel Configs Directory:    $kdir
   Initial Palacios Memory (MB) $mem
   Can Hot Remove:              $canhotremove
   Will Hot Remove:             $hotremove
   Enforce 4 GB Limit:          $shadow
   Compiled Memory Block Size:  $compmemblocksize
   Override Memory Block Size:  $override_memblocksize
   Actual Memory Block Size:    $memblocksize
   Allow Devmem:                $devmem

END2
;


#
#
#
#
print "Writing ./ENV\n";
open(ENV,">ENV");
print ENV "export PATH=\$PATH:$pdir/linux_usr\n";
close(ENV);
`chmod 644 ENV`;

print "Writing ./v3_init\n";
open(INIT,">v3_init");
print INIT "#!/bin/sh\n";
print INIT "source $pdir/ENV\n";  # just in case

print INIT "\n\n# insert the module\n";
$cmd = "insmod $pdir/v3vee.ko";
$cmd.= " allow_devmem=1 " if $devmem eq 'y';
$cmd.= " options=\"mem_block_size=$memblocksize\" " if $override_memblocksize eq 'y';
print INIT $cmd, "\n";

$cmd = "v3_mem";
$cmd.= " -k " if $hotremove eq 'n';
$cmd.= " -l " if $shadow eq 'y';

$chunk = $memblocksize / (1024 * 1024) ;
$numchunks = $mem / $chunk;
for ($i=0;$i<$numchunks;$i++) {
  print INIT "$cmd $chunk\n";
}
close(INIT);
`chmod 755 v3_init`;

print "Writing ./v3_deinit\n";
open(DEINIT,">v3_deinit");
print DEINIT "#!/bin/sh\n";
print DEINIT "echo WARNING - THIS DOES NOT CURRENTLY ONLINE MEMORY\n";
print DEINIT "rmmod v3vee\n";
close(DEINIT);
`chmod 755 v3_deinit`;
print "Done.\n";



sub get_user {
  my $def = shift;
  
  my $inp = <STDIN>; chomp($inp);
  
  if ($inp eq "") { 
    return $def;
  } else {
    return $inp;
  }
}

sub get_kernel_feature {
  my $dir=shift;
  my $feature=shift;
  my $x;

  $x=`grep $feature $dir/config-\`uname -r\``;

  if ($x=~/^\s*\#/) {
    return undef;
  } else {
    if ($x=~/\s*$feature\s*=\s*(\S*)\s*$/) {
      return $1;
    } else {
      return undef;
    }
  }
}
  
sub get_palacios_core_feature {
  my $dir=shift;
  my $feature=shift;
  my $x;

  $x=`grep $feature $dir/.config`;

  if ($x=~/^\s*\#/) {
    return undef;
  } else {
    if ($x=~/\s*$feature\s*=\s*(\S*)\s*$/) {
      return $1;
    } else {
      return undef;
    }
  }
}


sub powerof2  {
  my $x = shift;
  my $exp;
  
  $exp = log($x) /log(2);

  return $exp==int($exp);
}
