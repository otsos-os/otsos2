#!/usr/bin/perl
use strict;
use warnings;

my $file = $ARGV[0];
if (!$file || ! -f $file) {
    exit 0;
}

open(my $fh, '<', $file) or exit 0;
my $in_section = 0;
while (my $line = <$fh>) {
    chomp($line);
    if ($line =~ /^\s*\[modules\]/) {
        $in_section = 1;
        next;
    }
    if ($line =~ /^\s*\[/) {
        $in_section = 0;
        next;
    }
    if ($in_section) {
        if ($line =~ /^\s*#/) {
            next;
        }
        if ($line =~ /^\s*([^=\s#]+)\s*=/) {
            print "$1\n";
        }
    }
}
close($fh);
