#!/usr/bin/perl -w

use IO::Handle;
use Getopt::Long;
use Time::HiRes qw /usleep/;

$period=0;
$numtimes=1;

&GetOptions("period=i"=>\$period, "numtimes=i"=>\$numtimes);

$#ARGV==0 or die "v3_guest_mem_track_viz.pl [--numtimes=num(-1=forever)] [--period=milliseconds] /dev/v3-vmN\n";

$vm=shift;

open(G,"|gnuplot") or die "Cannot open gnuplot\n";
G->autoflush(1);
STDIN->autoflush(1);

while ($numtimes--) { 

  $data = `v3_guest_mem_track $vm snapshot text _`;
  
  $data=~/Cores:\s*(\d+)/ or die "Cannot parse for cores\n";
  $numcores=$1;
  
  $data=~/Pages:\s*(\d+)/ or die "Cannot parse for pages\n";
  $numpages=$1;
  
  #print $numpages;

  for ($core=0;$core<$numcores;$core++) {
    $data=~/Core\s+$core\s+\(.*\)\s+\:\s+(\S+)/ or die "Cannot parse core $core\n";
    $bits[$core]=$1;
  }
  
  $side=int(sqrt($numpages));
    
  $x=$side;
  $y=int($numpages/$side);

  print G "set xrange [0:$x]\n";
  print G "set yrange [0:$y]\n";  
  
  print G "plot ", join(",", map {"'-' using 1:2 with points title 'core $_' "} (0..$numcores-1) ),"\n";
  for ($core=0;$core<$numcores;$core++) {
    for ($i=0;$i<$numpages;$i++) {
      #  print substr($bits,$i,1);
      if (substr($bits[$core],$i,1) eq "X") { 
     	print G join("\t", int($i/$side), $i % $side), "\n";
      }
    }
    print G "e\n";
  }
 
  if ($period) {  
    usleep($period*1000);
  }
}

print "Hit enter to finish\n";
<STDIN>;
