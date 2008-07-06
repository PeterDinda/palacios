#!/usr/bin/perl



$dev_root = `pwd`;
#
# On cygwin, do something like the following
# pwd behaves trangely on cygwin
#
#$dev_root='/home/pdinda/Codes/vmm-dev';
chomp $dev_root;
$location = $dev_root . "/devtools";


print "\n";
print "Installing and configuring the v3vee development environment...\n";
print "Location: " . $location . "\n\n";

mkdir $location;



#install_binutils_2_16_91_i386();
install_gcc_3_4_6_i386();
install_nasm();
#
# This part is not quite working yet
# on cygwin, do through it by hand      
#
#install_dev86_0_16_17();


sub install_nasm {
  ### Setup vmx capable nasm
  print "Installing VMX capable NASM...\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf nasm-0.98.39.tar.gz`;
  print "done\n";
  chdir "./nasm-0.98.39";
  print "Patching in VMX support\n";
  `patch < ../vmx.patch`;
  print "Patch to fix the stupid installer...\n";
  `patch < ../nasm-install.patch`;
  print "Configuring...\n";
  `./configure --prefix=$location`;
  print "Compiling...\n";
  `make`;
  print "Installing...";
  `make install`;
  print "Done!!\n\n";
  chdir $dev_root;
}



sub install_binutils_2_16_91_i386 {
  print "Installing binutils v2.16.91\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf binutils-2.16.91.0.7.tar.gz`;
  print "done\n";
  chdir "binutils-2.16.91.0.7";
  print "Configuring...";
  `./configure --prefix=$location/i386 --target=i386-elf --disable-nls`;
  print "done\n";
  print "Compiling...";
  `make -j 4 all`;
  print "done\n";
  print "Installing...";
  `make install`;
  print "done!!\n";
  chdir $dev_root;
}


sub install_gcc_3_4_6_i386 {
  install_binutils_2_16_91_i386();
  $ENV{'PATH'} = "$location/i386/bin:" . $ENV{'PATH'};

  print "Installing gcc v3.4.6\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf gcc-3.4.6.tar.gz`;
  print "done\n";
  chdir "gcc-3.4.6";
  print "Configuring...";
  `./configure --prefix=$location/i386 --target=i386-elf --disable-nls --enable-languages=c,c++ --without-headers`;
  print "done\n";
  print "Compiling...";
  `make -j 4 all-gcc`;
  print "done\n";
  print "Installing...";
  `make install-gcc`;
  print "done!!\n";
  chdir $dev_root;
}


sub install_binutils_2_16_91_x86_64 {

}


sub install_gcc_3_4_6_x86_64 {

  print "Installing gcc v3.4.6\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf gcc-3.4.6.tar.gz`;
  print "done\n";
  chdir "gcc-3.4.6";
  print "Configuring...";
  `./configure --target=x86_64 --prefix=$location/gcc_3.4.6-x86_64 --disable-nls --enable-languages=c,c++ --without-headers`;
  print "done\n";
  print "Compiling...\n";
  `make all-gcc`;
  print "done\n";
  print "Installing...\n";
  `make install-gcc`;
  print "done!!\n";
  chdir $dev_root;
}

sub install_dev86_0_16_17 {

  print "Installing bcc, ld86, as86, and bcc-cpp from Dev86src-0.16.17.tar.gz\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf Dev86src-0.16.17.tar.gz`;
  print "done\n";
  chdir "./dev86-0.16.17";
  print "Compiling...\n";
  `make as86 ld86 bcc`;
  `make -C cpp`;
  print "done\n";
  print "Installing...\n";
  `cp as/as86.exe bcc/bcc.exe bcc/bcc-cc1.exe cpp/bcc-cpp.exe ld/ld86.exe make install-gcc $location/bin`;
  print "done!!\n";
  chdir $dev_root;
}
