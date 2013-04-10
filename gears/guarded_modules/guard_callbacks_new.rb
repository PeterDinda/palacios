#!/usr/bin/ruby

# Kyle C. Hale (c) 2012/2013
#
# Ruby implementation of a tool to identify the callbacks
# in a driver given the source for the driver and it's compiled kernel
# object. It does this by finding all static functions in the module and scanning
# the source to identify sites where a static function is being assigned to a variable.
#
# Dependencies: 
#               json ruby gem
#
# By default, all callbacks will be added as valid entry points into the module.
# For now, entries must manually be added and deleted by module author
#
# TODO:     
#           - remove hardcoded directories from this and chirsto script
#
#
# NOTES:
#
# - .i files are generated to confirm callback functions
#
# - source files and makefiles must be in same dir. Wont work for big
#   multi-level hierarchical source trees
#
# - this produces JSON output, including valid entry points, which 
#   (both entering into callback functions and returning from calls
#   to the kernel) can be modified by driver developer
#
# - this modifies the original driver source, as well as the original
#   driver Makefile
#
# - You must provide your **GUEST'S** System.map file for this script to reference

require 'optparse'
require 'rubygems'
require 'json'

CHRISTO = "/root/kyle/kyle/bnx/christo-s.pl"
GM_INIT = "v3_guard_mod_init"
GM_INIT_INTERNAL = "__guard_mod_init_internal"
GM_MACRO = "GM_ENTRY_REQUEST"
WRAP_FILE = "entry_wrapper.S"

GUARD_INIT_NR  = 0x6000
GUARD_ENTER_NR = 0x6001
GUARD_EXIT_NR  = 0x6002


def replace_callback_assigns(callbacks, file)

end

# ================== OPTIONS ========================= 

options = {}

optparse = OptionParser.new do|opts|

  opts.banner = "Usage: guard_modules.rb [options] ..."

  options[:verbose] = false
  opts.on( '-v', '--verbose', 'Allow verbose output') do
      options[:verbose] = true
  end
  
  opts.on( '-h', '--help', 'Show this information') do
      puts opts
      exit
  end

  opts.on('-s', '--source <driver,src,files,...,>', '(C) source code for driver module') do |file|
      options[:srcfile] = file.split(',')
  end
  
  opts.on('-p', '--privs <requested,privileges>', 'Privileges requested. Currently numerical') do |privs|
      options[:privs] = privs.split(',')
  end

  opts.on('-k', '--ko DRIVER KO', 'The compiled and linked kernel object for the driver') do |file|
      options[:ko] = file
  end
  
  opts.on('-m', '--makefile MAKEFILE', 'The driver\'s makefile') do |file|
        options[:mf] = file
  end

  opts.on('-o', '--output OUTFILE', 'The file to which static module info will be written') do |file|
    options[:out] = file
  end

  opts.on('-n', '--name NAME', 'The name of the guarded module') do |name|
    options[:name] = name
  end

  options[:instr] = false
  opts.on('i', '--instrument', 'Dump out the functions which are *NOT* to be instrumented, i.e. those that aren\'t callbacks')  do
    options[:instr] = true
  end
end


begin
  optparse.parse!
  mand = [:ko, :srcfile, :mf, :name]
  missing = mand.select{ |parm| options[parm].nil? }
  if not missing.empty?
    puts "Missing options: #{missing.join(', ')}"
    puts optparse
    exit
  end
rescue OptionParser::InvalidOption, OptionParser::MissingArgument
  puts $!.to_s
  puts optparse
  exit
end

# ===================== END OPTIONS ==============================

$mkdir   = File.expand_path(File.dirname(options[:mf]))
$mkbase  = File.basename(options[:mf])
#TODO: this needs to be specified explicitly
$srcpath = options[:srcfile][0]
$srcdir  = File.expand_path(File.dirname(options[:srcfile][0]))
$srcbase = File.basename(options[:srcfile][0])

# global list of functions
# TODO: THIS IS NOT WORKING!!!! e.g., on bnx2_find_max_ring, objdump is not finding static funs
funcs = `objdump -t #{options[:ko]} | awk '\$3==\"F\" {print \$6}'`

# only static functions
stats = `objdump -t #{options[:ko]} | awk '\$2==\"l\"&&\$3==\"F\" {print \$6}'`

callbacks = []

puts "Running preprocessor..."
prep_files = {}

Dir.chdir($mkdir) do

    # generate .i files (preprocessing)
    ENV['PWD'] = Dir.pwd

    abort("Problem cleaning") if not system "make clean > /dev/null"

    #TODO parameterize this!
    kerndir = "/v-test/guard_modules/gm_guest/kyle_gl/"

    # run the kernel make process, and repeat the build of each .o with preprocessing directives
    cmds = `make V=1 2>&1 | grep -P "gcc.*?\\s+\\-o\\s+.*?[^mod]\\.c" | sed 's/\\s\\+-c/ -E/g'| sed 's/\\(-o\\s\\+.*\\?\\)\\.o/\\1.i/g'`

    Dir.chdir(kerndir) do
        ENV['PWD'] = Dir.pwd

        cmds.each do |i| 
            abort ("Problem compiling.") if not system i
        end
    end

    ENV['PWD'] = Dir.pwd

    # read in preprocessed files
    Dir.glob("#{$mkdir}/*.i", File::FNM_DOTMATCH).each do |fname|
            prep_files[fname] = IO.readlines(fname)
            puts "Reading in preprocessed file: #{fname}"
            if not prep_files[fname]
                puts "could not open #{fname}"
                exit
            end
    end
end

ENV['PWD'] = Dir.pwd

stats.each do |stat|
  s = stat.chomp
  defs = []
  
  # look for callback assignments, this tells us that this
  # static function is indeed being used as a callback
  prep_files.each_value do |src|
    tmp_defs = src.grep(/.*=\s*#{s}\s*(;|,)/)
    defs.concat(tmp_defs) if not tmp_defs.empty?
  end

  callbacks.push(s) if not defs.empty?

  if (options[:verbose]) 
    puts "Possible callback assignment for #{s} : " if not defs.empty?
    defs.each {|i| print "\t#{i}\n"}
  end
  
end

if (options[:instr])
    f = funcs.split("\n")
    
    puts "Searching for driver's module_init routine..."
    mod_init = nil

    # TODO: make this work for multiple files
    File.open($srcpath, 'r') do |file|
        new_str = file.read
        if new_str =~ /module_init\((.*?)\)/ 
            puts "Found it, it's #{$1}" 
            mod_init = $1
        end
    end
    
    # back up (TODO: ALL) source/makefiles
    `cp #{$mkdir}/#{$mkbase} #{$mkdir}/#{$mkbase}.bak`
    `cp #{options[:srcfile][0]} #{$srcdir}/#{$srcbase}.bak`

    # generate a restore script
    shell = `which sh`
    restore = <<-eos
#!#{shell}
for i in `ls *.bak | sed 's/\.bak//g'`; do mv $i.bak $i; done
make clean && make
rm -f *.i
eos

    # TODO: WARNING WARNING: assumes source files and makefiles are in same place
    File.open("#{$srcdir}/gm_restore.sh", "w", 0744) do |file|
        file.puts restore 
    end

    str = <<-eos
/* BEGIN GENERATED */
#define V3_GUARD_ENTER_HCALL_NR #{GUARD_ENTER_NR}
#define V3_GUARD_EXIT_HCALL_NR  #{GUARD_EXIT_NR}
#define V3_GUARD_INIT_HCALL_NR  #{GUARD_INIT_NR}
#define V3_GUARDED_MODULE_ID   0xA3AEEA3AEEBADBADULL
#define #{GM_MACRO}(fun) _gm_entry_req_##fun
int __init #{GM_INIT} (void);
eos

# for assembly linkage
callbacks.each do |c| 
    str +=<<-eos "int _gm_entry_req_#{c}(void);\n"
__asm__(".globl _gm_entry_req_#{c};\\
   _gm_entry_req_#{c}:\\
   popq  %r11;          \\
   pushq %rax;\\
   movq  $#{GUARD_ENTER_NR}, %rax;\\
   vmmcall;\\
   popq  %rax;\\
   callq #{c};\\
   pushq %rax;\\
   movq  $#{GUARD_EXIT_NR}, %rax;\\
   vmmcall;\\
   popq  %rax;\\
   pushq %r11;\\
   ret;");
eos
end
str += "/* END GENREATED */\n"

end_str = <<-eos
/* BEGIN GENERATED */
int #{GM_INIT_INTERNAL} (void) __attribute__((section (".text"),no_instrument_function));

int #{GM_INIT_INTERNAL} (void) {
        int ret, ret_orig;

        /* GUARD INIT */
        __asm__ __volatile__ ("vmmcall"
                      :"=a" (ret)
                      :"0"  (V3_GUARD_INIT_HCALL_NR), "b" (V3_GUARDED_MODULE_ID));

        if (ret < 0) {
                printk("Guest GM: error initializing guarded module\\n");
        } else {
                printk("Guest GM: successfully initialized guarded module\\n");
        }

        ret_orig = #{mod_init}();

        if (ret_orig < 0) { 
                printk("Guest GM: error calling original init\\n");
        } else {
                printk("Guest GM: successfully called original driver init\\n");
        }

        /* GUARD EXIT */
        __asm__ __volatile__("vmmcall" : "=a" (ret) : "0" (V3_GUARD_EXIT_HCALL_NR));

        if (ret < 0) {
                printk("Guest GM: error doing initial exit\\n");
                return ret;
        }

        return ret_orig;
}

int __init #{GM_INIT} (void) {
        return #{GM_INIT_INTERNAL}();
}

/* END GENERATED */
eos

# GENERATE THE CALLBACK WRAPPERS
#
# statements like 
# .callback = myfunc;
#
# will be replaced with 
# .callback = GM_ENTRY_REQUEST(myfunc);
#
# which expands to 
# .callback = __gm_entry_req_myfunc;  (this is an assembly stub)
#

    callbacks.each do |c|
      end_str += <<-eos
      void  (*#{c}_ptr)() = #{c};
eos
    end

    end_str += "/* END GENERATED */"

    # append instrumentation functions to end of source file
    new_str = ""
    File.open($srcpath, "r") do |file|
        new_str = file.read
    end

    # fixup module init calls with our own
    new_str = new_str.gsub(/__init #{mod_init}/, "#{mod_init}")
    new_str = new_str.gsub(/module_init\(.*?\);/, "module_init(#{GM_INIT});")

    # fixup callback assignments with our macro
    # TODO: make this work for multiple source files
    callbacks.each do |c|
        new_str = new_str.gsub(/(.*=\s*)#{c}(\s*(;|,))/) {
            "#{$1}#{GM_MACRO}(#{c})#{$2}"
        }
    end

    # put these at the top of the file, after the includes
    new_str = new_str.insert(new_str.rindex(/#include.*?[>"]/), str)
    new_str += end_str
    
    File.open($srcpath, "w+") do |file|
        file.puts new_str
    end

    Dir.chdir($mkdir) do
        abort ("Error cleaning") if not system "make clean > /dev/null 2>&1"
        abort("Error rebuilding module") if not system "make > /dev/null 2>&1"
    end

end

# if an outfile is specified, output static module info
# in JSON format
if (options[:out])
  # First generate wrapped module

  ko = File.expand_path(options[:ko])
  puts "Running christo with Kernel object: " + ko
  christo_output = `#{CHRISTO} --kern #{ko} 2>&1 | grep ACCEPT`
  out_arr = []
  christo_output.each_line { |x| out_arr.push(x.chomp.gsub(/\s*ACCEPT\s*/, '')) }

  wrap_name = File.basename(ko).sub(/\.ko$/,'_wrapped.ko')

  ret_points = []
  out_arr.each do |x|
      ret_points.push Array[x, `objdump -d #{wrap_name} | sed -n '/__wrap_#{x}>/,/ret/p' | grep vmmcall | tail -1 | cut -f1 -d':' | sed 's/\s//g'`.to_i(16)]
  end

  abort("Problem building wrapped module") if not system "make > /dev/null 2>&1"
  static_info = {}
  static_info['module_name'] = options[:name]

  # get the content hash of .text
  static_info['content_hash'] = `objdump -h #{wrap_name} | grep -P "\s+\.text\s+" | awk '{print "dd if=#{wrap_name} bs=1 count=$[0x" $3 "] skip=$[0x" $6 "] 2>/dev/null"}' | bash | md5sum - | awk '{print $1}'`.chomp
  static_info['size'] = `objdump -h #{wrap_name} | grep -P "\s+\.text\s+" | awk '{print $3}'`.to_i(16)

  # find offset to hcall (we've ensured it's in .text)
  static_info['hcall_offset'] = `objdump -d #{wrap_name} | sed -n '/#{GM_INIT_INTERNAL}/,/ret/p'| grep vmmcall | cut -f1 -d':'|sed 's/\s//g'`.to_i(16)

  # each callback function will have ONE valid entry point, the address of the VMMCALL
  # in its function body
  static_info['entry_points'] = callbacks.collect do |c|
      c_off = `objdump -d #{wrap_name} | sed -n '/_gm_entry_req_#{c}>/,/vmmcall/p' | tail -1 | cut -f1 -d':' | sed 's/\s//g'`.to_i(16)
      Array[c, c_off]
  end

  static_info['ret_points'] = ret_points
  static_info['privileges'] = options[:privs]

  File.open(options[:out], "w") do |file|
      file.write(JSON.pretty_generate(static_info))
  end

else
  callbacks.each {|c| puts c }
end
