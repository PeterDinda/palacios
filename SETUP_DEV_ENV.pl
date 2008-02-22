#!/usr/bin/perl



$location = `pwd`;
chomp $location;
$location .= "/devtools";


print "\n";
print "Installing and configuring the v3vee development environment...\n";
print "Location: " . $location . "\n\n";

mkdir $location;


install_nasm();


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
}
