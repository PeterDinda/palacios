package v3_checkpoint_file;

require Exporter;
@ISA = qw(Exporter);

@EXPORT = qw(v3_read_checkpoint_from_binary_file
	     v3_read_checkpoint_from_binary_dir
             v3_read_checkpoint_from_ini_file
	     v3_read_checkpoint_from_ini_dir
             v3_write_checkpoint_as_ini_file
             v3_write_checkpoint_as_ini_dir
             v3_write_checkpoint_as_binary_dir);

my $boundary=0xabcd0123;


#
# Takes: filename
#
#
# Returns:
# hashref
#   tags=>[list of tags in file, in order]
#   tagname=>blob
#   ..
#
# dies if it doesn't work
#
sub v3_read_checkpoint_from_binary_file {
  $file = shift;
  my %h;
  my $buf;
  my $bt;
  my $taglen;
  my $datalen;
  my $curtag;

  $h{tags}=[];

  open(FILE,$file) or die "cannot open $file\n";
  binmode(FILE);

  while (1) { 
    undef $buf;
    if (sysread(FILE, $buf, 4)!=4) { 
      close(FILE);
      return \%h;
    }
    $bt=unpack("L",$buf);
    $bt==$boundary or die "Boundary mismatch (read $bt)\n";
    undef $buf;
    sysread(FILE,$buf,8)==8 or die "Can't read taglen\n";
    $taglen=unpack("Q",$buf);
    undef $curtag;
    sysread(FILE,$curtag,$taglen)==$taglen or die "Can't read tag\n";
    undef $buf;
    sysread(FILE,$buf,8)==8 or die "Can't read datalen\n";
    $datalen=unpack("Q",$buf);
    undef $buf;
    sysread(FILE,$buf,$datalen)==$datalen or die "Can't read data\n";
    push @{$h{tags}},$curtag;
    $h{$curtag}=$buf;
  }
}

sub inlist {
  my ($key,@list)=@_;
  my $k;
  foreach $k (@list) { 
    return 1 if ($k eq $key);
  }
  return 0;
}

# Takes: dirname [optional list of keys/files to skip]
# Returns:
# hashref
#   keys=>[list of keys in dir, in order]
#   keyname => [ tags=>[list of tags in file, in order], 
#               tagname=>blob
#               ..
#
# dies if it doesn't work
#
sub v3_read_checkpoint_from_binary_dir {
  my ($dir,@skiplist)=@_;
  my @files = `ls -1 $dir`; chomp(@files);
  my $file;
  my $key;
  my %h;

  $h{keys}=[];;
  
  foreach $file (@files) { 
    $key=$file;
    next if inlist($key,@skiplist);
    $h{$key} = v3_read_checkpoint_from_binary_file("$dir/$file");
    push @{$h{keys}}, $key;
  }
  
  return \%h;
}

#
# Takes: filename
#
#
# Returns:
# hashref
#   tags=>[list of tags in file, in order]
#   tagname=>blob
#   ..
#
# dies if it doesn't work
#
sub v3_read_checkpoint_from_ini_file {
  $file = shift;
  my %h;
  my $line;
  my $key=undef;

  $h{tags}=[];

  open(FILE,$file) or die "cannot open $file\n";

  # find key
  while ($line=<FILE>) { 
    chomp($line);
    if ($line=~/\[(.*)\]$/) {
      $key=$1;
      last;
    } 
  }
  defined($key) or die "cannot find key\n";
  
  while ($line=<FILE>) { 
    chomp($line);
    if ($line=~/^(.*)\=(.*)$/) { 
      $h{$1}=pack("H*",$2);
      push @{$h{tags}}, $1;
    } else {
      # skip
    }
  }
  close(FILE);

  return \%h;
}


# Takes: dirname [optional list of keys/files to skip]
# Returns:
# hashref
#   keys=>[list of keys in dir, in order]
#   keyname => [ tags=>[list of tags in file, in order], 
#               tagname=>blob
#               ..
#
# dies if it doesn't work
#
sub v3_read_checkpoint_from_ini_dir {
  my ($dir,@skiplist)=@_;
  my @files = `ls -1 $dir`; chomp(@files);
  my $file;
  my $key;
  my %h;

  $h{keys}=[];;
  
  foreach $file (@files) { 
    $key=$file;
    next if inlist($key,@skiplist);
    $h{$key} = v3_read_checkpoint_from_ini_file("$dir/$file");
    push @{$h{keys}}, $key;
  }

  return \%h;
}


sub v3_write_checkpoint_as_ini_stream {
  my ($key,$hr,$stream)=@_;
  my $tag;

  print $stream "[$key]\n" if defined($key);

  foreach $tag (@{$hr->{tags}}) { 
    print $stream "$tag=".unpack("H*",$hr->{$tag})."\n";
  }
}

#
# input: href, either for individual key, or group of keys
#        file to write
#        [key optional for single key]
#
sub v3_write_checkpoint_as_ini_file {
  my ($hr,$file,$key)=@_;

  open(FILE, ">$file") or die "cannot open $file\n";

  if (defined($hr->{keys})) { 
    # this is a whole checkpont we are being asked to write
    foreach $key (@{$hr->{keys}}) { 
      v3_write_checkpoint_as_ini_stream($key,$hr->{$key},FILE);
    }
  } else {
    # this is an individual file, and we don't know it's keyname.  
    # caller must have already wrote it.
    v3_write_checkpoint_as_ini_stream($key,$hr,FILE);
  }
  close(FILE);
}

#
# input: href, either for individual key, or group of keys
#        stream to write
#
#
sub v3_write_checkpoint_as_ini_dir {
  my ($hr,$dir)=@_;
  my $file;
  my $key;

  defined($hr->{keys}) or die "cannot write dir without key list\n";
  
  mkdir($dir); # OK if it already exists

  foreach my $key (@{$hr->{keys}}) { 
    open(FILE,">$dir/$key") or die "cannot open $dir/$key\n";
    v3_write_checkpoint_as_ini_stream($key,$hr->{$key},FILE);
    close(FILE);
  }
}

#
# input: href, either for individual key, or group of keys
#        file to write
#
sub v3_write_checkpoint_as_binary_file {
  my ($hr,$file)=@_;
  my $tag;
  my $taglen;
  my $datalen;
  my $buf;

  open(FILE, ">$file") or die "cannot open $file\n";
  binmode(FILE);

  foreach $tag (@{$hr->{tags}}) { 
    $buf=pack("L",$boundary);
    syswrite(FILE,$buf,4)==4 or die "cannot write boundary tag\n";
    $taglen=length($tag);
    $buf=pack("Q",$taglen);
    syswrite(FILE,$buf,8)==8 or die "cannot write tag length\n";
    syswrite(FILE,$tag,$taglen)==$taglen or die "cannot write tag\n";
    $datalen=length($hr->{$tag});
    $buf=pack("Q",$datalen);
    syswrite(FILE,$buf,8)==8 or die "cannot write data length\n";
    syswrite(FILE,$hr->{$tag},$datalen)==$datalen or die "cannot write data\n";
  }
  close(FILE);
}

#
# input: href, either for individual key, or group of keys
#        stream to write
#
#
sub v3_write_checkpoint_as_binary_dir {
  my ($hr,$dir)=@_;
  my $file;
  my $key;

  defined($hr->{keys}) or die "cannot write dir without key list\n";
  
  mkdir($dir); # OK if it already exists

  foreach my $key (@{$hr->{keys}}) { 
    v3_write_checkpoint_as_binary_file($hr->{$key},"$dir/$key");
  }
}

