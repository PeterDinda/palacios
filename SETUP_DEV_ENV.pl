#!/usr/bin/perl



$location = `pwd`;
chomp $location;
$location .= "/devtools";


print "\n";
print "Installing and configuring the v3vee development environment...\n";
print "Location: " . $location . "\n\n";

mkdir $location;


#install_nasm();
install_gcc_3_4_6();


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
  chdir $location;
}



sub install_binutils_2_16_91_i386 {
  print "Installing binutils v2.16.91\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf binutils-2.16.91.0.7.tar.gz`;
  print "done\n";
  chdir "binutils-2.16.91.0.7";
  print "Configuring...";
  `./configure --prefix=$location --target=i386-elf`;
  print "done\n";
  print "Compiling...";
  `make`;
  print "done\n";
  print "Installing...";
  `make install`;
  print "done!!\n";
  chdir $location;

}


sub install_gcc_3_4_6_i386 {
  print "Installing gcc v3.4.6\n";
  chdir "./utils";
  print "Unpacking...";
  `tar -xzf gcc-3.4.6.tar.gz`;
  print "done\n";
  chdir "gcc-3.4.6";
  print "Configuring...";
  `./configure --prefix=$location`;
  print "done\n";
  print "Compiling...";
  `make`;
  print "done\n";
  print "Installing...";
  `make install`;
  print "done!!\n";
  chdir $location;
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
  chdir $location;
}
