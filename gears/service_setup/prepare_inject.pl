#! /usr/bin/perl -w

use Getopt::Long;

sub usage() {
	die "\n\nusage: prepare_inject.pl [-w output_file_name inject_object] [-e command {arg_list} ]\n\n".
	    "You must either indicate to write out an injected file with -w or to execute a command,".
	    " with -e, or both.\n\n".
	    "\t'output_file_name' is what the name of the inject_object will be when it is written out to the guest.\n\n".
   	    "\t'inject_object' is the file that will be written out to the guest. This could be a text file, program, or ".
	    "really anything.\n\n".
	    "\t'command' is the fully qualified path name for a file within the guest to execute, either by itself, ".
	    "or after a specified inject_object is written out.\n\n";
}

&GetOptions("w:s{2}" => \@write_opts, "e:s{,}" => \@exec_opts, "output:s" => \$out_name) or usage();

usage() unless (@exec_opts || @write_opts);

$hfile = <<END;
#ifndef _GENERATED_H_
#define _GENERATED_H_

END


if (@exec_opts) {
	$cmd = $exec_opts[0];
	$hfile .= "#define DO_FORKEXEC\n"; 
	$hfile .= "#define CMD \"$cmd\"\n";

	$numargs = scalar(@exec_opts);
	$hfile .= "char * const args[".$numargs."] = {\"".join('","', @exec_opts)."\"};\n";
}


if (@write_opts) {
	$out_file = $write_opts[0];
	$inject_file = $write_opts[1];
	
	$hfile .= "#define DO_WRITE\n";
	$hfile .= "#define FILE_NAME \"$out_file\"\n";
	

	$size = `ls -l $inject_file | cut -f5 -d ' '`;
	$hfile .= "#define FILE_LENGTH $size\n";

	# generate a string from the file, char * inject_file = string
	open FILE, $inject_file or die $!;
	binmode FILE;
	my ($buf, $data, $n);
	while (($n = read FILE, $data, 1) != 0) {
	  $buf .= "\\x" . unpack("H8", $data);
	}
	
	close(FILE);
	$hfile .= "char * inject_file = \"$buf\";\n\n\n";
}

$hfile .= "#endif\n";

# write out the h file
open (W, ">generated.h") or die $!;
print W $hfile;
close(W);

print "running special inject code compilation and linking...\n";
# compile with generated h file and inject_code_template.c with peter's script
$compile_cmd = "perl compile-for-static-user-level-injection.pl -32 ";
$compile_cmd .= "--output=$out_name " if defined($out_name);
$compile_cmd .= "inject_code_template.c";
system($compile_cmd);

unlink "generated.h";


print "All done.\n";

