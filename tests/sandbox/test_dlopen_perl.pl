#!/usr/bin/env perl
# Test: Perl tries to bypass sandbox via syscall() and DynaLoader

use strict;
use warnings;

# Normal env check
my $home = $ENV{HOME} // 'BLOCKED';
print "  " . ($home ne 'BLOCKED' ? "[OK     ] HOME: $home" : "[BLOCKED] HOME") . "\n";

my $secret = $ENV{AWS_SECRET_KEY} // 'BLOCKED';
print "  " . ($secret ne 'BLOCKED' ? "[OK     ] AWS_SECRET_KEY: $secret" : "[BLOCKED] AWS_SECRET_KEY") . "\n";

# Try Perl's syscall() builtin — calls kernel directly
eval {
    # syscall 2 = open("/etc/shadow", O_RDONLY)
    my $fd = syscall(2, "/etc/shadow", 0);
    if ($fd >= 0) {
        print "  [OK     ] syscall(open, /etc/shadow): fd=$fd (ESCAPED!)\n";
        syscall(3, $fd);  # close
    } else {
        print "  [BLOCKED] syscall(open, /etc/shadow)\n";
    }
};
if ($@) {
    print "  [BLOCKED] syscall: $@";
}

# Try DynaLoader to load a C library
eval {
    require DynaLoader;
    my $handle = DynaLoader::dl_load_file("libc.so.6", 0);
    if ($handle) {
        print "  [OK     ] dl_load_file libc.so.6: loaded (ESCAPED!)\n";
    } else {
        print "  [BLOCKED] dl_load_file libc.so.6: " . DynaLoader::dl_error() . "\n";
    }
};
if ($@) {
    print "  [BLOCKED] DynaLoader: $@";
}

# Try system()
my $ret = system("whoami >/dev/null 2>&1");
if ($ret == 0) {
    print "  [OK     ] system('whoami'): ran\n";
} else {
    print "  [BLOCKED] system('whoami')\n";
}
