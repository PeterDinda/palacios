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
  ./v3_config_v3vee.pl


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

$gb4 = 'n';

print "Do you need to use features that require 4GB enforcement of page allocation? [$gb4] : ";

$gb4 = get_user($gb4);

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

$qemu='n';
$qemudir=$pdir."/linux_usr/qemu";

$hostdev = get_palacios_core_feature($pdir,"V3_CONFIG_HOST_DEVICE");

if ($hostdev eq "y") { 
   print "Your Palacios configuration includes the host device interface.\n";
   print "You can use it to interface with QEMU devices if you have a\n";
   print "patched version of QEMU (see linux_usr/qemu for more info)\n\n";
   print "Do you plan to use QEMU devices? [n] : ";
   $qemu = get_user($qemu);
   if ($qemu eq "y") {
    while (1) { 
        print "What is the path to your patched version of QEMU ? [$qemudir] : ";
        $qemudir = get_user($qemudir);
        last if -e "$qemudir/bin/qemu-system-x86_64";
        print "$qemudir/bin/qemu-system-x86_64 cannot be found\n";
      } 
    }
}

print <<END2

Parameters
   Palacios Direcotry:          $pdir
   Kernel Configs Directory:    $kdir
   Initial Palacios Memory (MB) $mem
   Can Hot Remove:              $canhotremove
   Will Hot Remove:             $hotremove
   Enforce 4 GB Limit:          $gb4
   Compiled Memory Block Size:  $compmemblocksize
   Override Memory Block Size:  $override_memblocksize
   Actual Memory Block Size:    $memblocksize
   Allow Devmem:                $devmem
   Support QEMU devices:        $qemu
   QEMU directory:              $qemudir

END2
;


#
#
#
#
print "Writing ./ENV\n";
open(ENV,">ENV");
print ENV "export PALACIOS_DIR=$pdir\n";
if ($qemu eq "y") { 
  print ENV "export PALACIOS_QEMU_DIR=$qemudir\n";
}
print ENV "export PATH=$pdir/linux_usr:\$PATH\n";
close(ENV);
`chmod 644 ENV`;

print "Writing ./v3_init\n";
open(INIT,">v3_init");
print INIT "#!/bin/sh\n";
print INIT "source $pdir/ENV\n";  # just in case

# this file will track memory allocations
# made at user level so that they can be recovered later
# v3_mem will append to it
print INIT "rm -f $pdir/.v3offlinedmem\n";

print INIT "\n\n# insert the module\n";
$cmd = "insmod $pdir/v3vee.ko";
$cmd.= " allow_devmem=1 " if $devmem eq 'y';
$cmd.= " options=\"mem_block_size=$memblocksize\" " if $override_memblocksize eq 'y';
print INIT $cmd, "\n";

%numa = get_numa_data();

if (defined($numa{numnodes})) {
  $numnodes=$numa{numnodes};
} else {
  $numnodes=1;
}

if (defined($numa{numcores})) { 
  $numcores=$numa{numcores};
  $numcorespalacios = get_palacios_core_feature($pdir,"V3_CONFIG_MAX_CPUS");
  if (defined($numcorespalacios) && $numcores>$numcorespalacios) { 
    print "WARNING: Your Palacios configuration is configured to support\n";
    print " a maximum of $numcorespalacios cores, but this machine has $numcores cores.\n";
    print " Your Palacios will work on this machine, but will not be able to use\n";
    print " the additional cores.\n";
    if ($numnodes>1) {
       print " This is also a NUMA machine, so this will also affect the initial\n";
       print " allocation of memory in the NUMA nodes produced by this script.\n";
       print " We highly recommend you reconfigure Palacios with at least $numcores cores and rebuild it.\n";
     }
  }
} 



$chunk = $memblocksize / (1024 * 1024) ;
$numchunks = $mem / $chunk;
$chunkspernode  = $numchunks / $numnodes;

print "The initial memory allocation will be:\n\n";
print "  Total memory:       $mem MB\n";
print "  Memory block size:  $chunk MB\n";
print "  Number of blocks:   $numchunks\n";
print "  Number of nodes:    $numnodes\n";
print "  Blocks/node:        $chunkspernode\n";
print "  32 bit limit?       $gb4\n";
print "  Hot-removed?        $hotremove\n";

if ($numnodes*$chunkspernode*$chunk != $mem) { 
  print "\nWARNING: The memory is not evenly divided among nodes or blocks.\n";
  print " This means that LESS memory is allocated than requested.\n\n";
}

$cmd = "v3_mem -a";
$cmd.= " -k " if $hotremove eq 'n';
$cmd.= " -l " if $gb4 eq 'y';

for ($i=0;$i<$numnodes;$i++) {
  for ($j=0;$j<$chunkspernode;$j++) { 
    if ($numnodes>1) { 
      print INIT "$cmd -n $i $chunk\n";
    } else {
      print INIT "$cmd $chunk\n";
    }
  }
}
close(INIT);
`chmod 755 v3_init`;

print "Writing ./v3_deinit\n";
open(DEINIT,">v3_deinit");
print DEINIT "#!/bin/sh\n";
# bring any offlined memory back to the kernel
print DEINIT "v3_mem -r offline\n";
# a side effect here is that the offline file will be empty
# the rmmod will free any in-kernel allocated memory
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

  $x=`grep $feature= $dir/.config`;

  if ($x=~/^\s*\#/) {
    return undef;
  } else {
    if ($x=~/\s*$feature=\s*(\S*)\s*$/) {
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

sub get_numa_data() {
  my $line;
  my $maxnode=0;
  my $maxcpu=0;
  my %numa;

  open (N, "numactl --hardware |");
  while ($line=<N>) { 
    if ($line=~/^node\s+(\d+)\s+cpus:\s+(.*)$/) { 
      my $node=$1;
      my @cpus = split(/\s+/,$2);
      my $cpu;
      if ($node>$maxnode) { $maxnode=$node; }
      foreach $cpu (@cpus) { 
	if ($cpu>$maxcpu) { $maxcpu=$cpu; }
      }
      $numa{"node$node"}{cores}=\@cpus;
    }
  }
  $numa{numnodes}=$maxnode+1;
  $numa{numcores}=$maxcpu+1;
  return %numa;
}
