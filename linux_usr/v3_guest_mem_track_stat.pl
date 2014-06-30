#!/usr/bin/perl -w

use IO::Handle;
use Getopt::Long;
use Time::HiRes qw / usleep / ;
use List::Util qw / min max /;

my $period=0;
my $numtimes=1;
my $i;
my $j;
my $core;
my $mhz=get_mhz();
my $topk=10;


&GetOptions("period=i"=>\$period, "numtimes=i"=>\$numtimes, "topk=i"=>\$topk);

$#ARGV==0 or die "v3_guest_mem_track_stat.pl [--numtimes=num(-1=forever)] [--period=milliseconds] [--topk=num] /dev/v3-vmN\n";

$vm=shift;

while ($numtimes--) { 

  $data = `v3_guest_mem_track $vm snapshot text _`;

  $data=~/Time:\s+(\d+)/ or die "Cannot parse for time\n";
  $time=$1;

  $data=~/Interval:\s+(\d+)\s+cycles/ or die "Cannot parse for time\n";
  $interval=$1;
  
  $data=~/Access:\s+(\S+)/ or die "Cannot parse for access\n";
  $access=$1;

  $data=~/Reset:\s+(\S+)/ or die "Cannot parse for reset\n";
  $reset=$1;
  
  $data=~/Cores:\s*(\d+)/ or die "Cannot parse for cores\n";
  $numcores=$1;
  
  $data=~/Pages:\s*(\d+)/ or die "Cannot parse for pages\n";
  $numpages=$1;
  
  #print $numpages;

  for ($core=0;$core<$numcores;$core++) {
    $data=~/Core\s+$core\s+\((\d+)\s+to\s+(\d+)\,\s+(\d+)\s+pages\s+touched\)\s+\:\s+(\S+)/ or die "Cannot parse core $core\n";
    $start[$core]=$1;
    $end[$core]=$2;
    $pages[$core]=$3;
    $bits[$core]=$4;
  }

  
  $earliest=min(@start);
  $latest=max(@end);

  print "\033[2J\033[0;0H"; # clear

  print "VM:\t$vm\n";
  print "MHz:\t$mhz\n";
  print "Time:\t$time seconds\n";
  print "Access:\t$access\n";
  print "Reset:\t$reset\n";
  print "Inter:\t$interval cycles (",$interval/$mhz/1000," ms)\n";
  print "Cores:\t$numcores\n";
  print "Pages:\t$numpages\n";
  print "Time:\t$earliest to $latest (",($latest-$earliest)/$mhz/1000," ms interval)\n\n";
  
  for ($core=0;$core<$numcores;$core++) {
    print "Core $core: $pages[$core] pages ($start[$core] to $end[$core])\n";
  }

  for ($i=0; $i<$numcores; $i++) { 
    for ($j=0; $j<=$i; $j++) { 
#      print "$i x $j\n";
      $common[$i][$j]=count(intersect($bits[$i],$bits[$j]));
    }
  }
  
  print "\nIntersection (count)\n\n";
  
  for ($i=0;$i<$numcores;$i++) { 
    print "\t$i";
  }
  print "\n";

  for ($i=0;$i<$numcores;$i++) { 
    print "$i : ";
    for ($j=0;$j<=$i;$j++) { 
      print  "\t",$common[$i][$j];
    }
    print "\n";
  }

  print "\nIntersection (%)\n\n";
  
  for ($i=0;$i<$numcores;$i++) { 
    print "\t$i";
  }
  print "\n";

  for ($i=0;$i<$numcores;$i++) { 
    print "$i :";
    for ($j=0;$j<=$i;$j++) { 
      print "\t", sprintf("%4.2f",($common[$i][$j]/$numpages));
    }
    print "\n";
  }

  %ph=page_hash(\@bits);

  @pageranks = sort { $#{$ph{$b}} != $#{$ph{$a}} ? ($#{$ph{$b}} <=> $#{$ph{$a}}) : $a<=>$b } keys %ph;

  print "\nPages by popularity and sharing cores (top-$topk)\n\n";

  print "Page\tCores\n";
  for ($i=0;$i<min($topk,$#pageranks+1);$i++) { 
    print $pageranks[$i],"\t", join(" ",@{$ph{$pageranks[$i]}}),"\n";
  }
  print "\n";
  


  if ($period) {  
    usleep($period*1000);
  }
}



  

sub count {
  my $data = shift;
  my $n = length($data);
  my $c =0;
  my $i;
  
  for ($i=0;$i<$n;$i++) {
    if ((substr($data,$i,1) eq "X")) { 
      $c++;
    }
  }

  return $c;
}


sub intersect {
  my ($left, $right)=@_;

  my $n;
  my $i;
  my $out="";

  $n=length($left);

  $out="." x $n;


  for ($i=0;$i<$n;$i++) {
    if ((substr($left,$i,1) eq "X") && (substr($right,$i,1) eq "X")) { 
      substr($out,$i,1)="X";
    } else {
      substr($out,$i,1)=".";
    }
  }

  return $out;
}


sub get_mhz  {
  my $data=`cat /proc/cpuinfo`;

  $data=~/cpu\s+MHz\s+:\s+(\S+)/ or die "Cannot find MHz\n";

  return $1;
}


sub page_hash {
  my $br=shift;
  my $i;
  my $j;
  my %h;

  for ($i=0;$i<=$#{$br};$i++) {
    for ($j=0;$j<length($br->[$i]);$j++) {
      if ((substr($br->[$i],$j,1) eq "X")) {
	push @{$h{$j}}, $i;
      }
    }
  }
   
  return %h;

}
