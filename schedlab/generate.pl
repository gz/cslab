#!/usr/bin/perl -w

# (c) 2006-2007, ETH Zurich
# Matteo Corti <matteo.corti@id.ethz.ch>
# Mathias Payer <mathias.payer@inf.ethz.ch>

use Getopt::Std;

my $name = $1;
my $procs;
my $trace;
my $i;
my $PID;
my $time;
my $priority;

my $MAX_DELAY    = 20;
my $MIN_DURATION = 10;
my $MAX_DURATION = 40;
my $RENICE_PROB  = 0.25;

use vars qw($opt_h $opt_v $opt_s $opt_p $opt_d $opt_t $opt_r);

################################################################################
# parse arguments
if (!getopts('hvsp:t:d:r:')) {
    usage();
}

if (defined $opt_h) {
    usage();
}

if (defined $opt_p) {
    $procs = $opt_p;
}
else {
    $procs = 1;
}

if (defined $opt_d) {
    $MAX_DURATION = $opt_d;
}

if (defined $opt_t) {
    $MAX_DELAY = $opt_t;
}

if (defined $opt_r) {
    $RENICE_PROB = $opt_r;
}
$RENICE_PROB = $RENICE_PROB*100;

if ($#ARGV != 0) {
    usage();
}

$name = $ARGV[0];

################################################################################
# open the trace file
if (defined $opt_v) {
    print "creating $name with $procs processes\n";
}

open(TRACE, "> $name") or die "Unable to open $name: $!\n";
$| = 1, select $_ for select TRACE; # unbuffered

srand();

##########################################################################################
# Generate p processes

$i    = 0;
$time = 0;
$PID  = 0;
while ($i < $procs) {
    
    # Start time (random, increasing)
    $time      += int rand $MAX_DELAY;
    
    # duration (random)
    $duration = int rand ($MAX_DURATION-$MIN_DURATION) + $MIN_DURATION;
    
    if (defined $opt_s) {

        # simple: no priorities
        $priority = 0;

    }
    else {

        # normal: priorities (random)
        $priority = int rand 3;
        
        # duration is inversely proportional to the priority
        $duration = $duration * (2**(2-$priority)) + 1;
        
    }
    print TRACE "$time start $PID $duration $priority\n";
    
    # eventual renice in the lifetime of the process
    if (!defined $opt_s && int rand 100 < $RENICE_PROB) {
        # relative to the starttime of the process
        # distribute the renice into starttime+1 to starttime+duration-1
        $ptime = (int rand ($duration-2)) + 1;
        print TRACE "$ptime renice $PID 0 $priority\n";
    }
    
    # generate some locks
    if (!defined $opt_s) {
        $j = 0;
        $res = 0;
        # relative to the starttime of the process
        $ptime = int rand ($duration / 10) + 1;
        $endtime = $duration;
        # we assume that when a process finishes all locks are removed
        while (($j < $duration / 10) && ($ptime < $endtime)) {
            $lock_duration = int rand ($duration / 5) + 1;
            if (($ptime + $lock_duration) < $endtime) { 
              print TRACE "$ptime lock $PID $lock_duration $res\n";
              $res++; # we lock in increasing order to avoid deadlocks
            }
            $j++;
            $ptime = $ptime + int rand ($duration / 10) + 1;
        }
    }

    $PID++;
    $i++;
}

close(TRACE);

1;

################################################################################
#
# Display the usage information
#
sub usage {

  print <<EOT;

    generate.pl -- Generate scheduling traces

    Usage:
          generate.pl [OPTION] file

    Options:
    -d time\tmax duration of a process
    -h\t\tprint this message
    -p procs\tnumber of proecesses
    -r prob\tprobability of a renice for a given process
    -s\t\tsimple traces (no locks, no priorities)
    -t time\tmax delay between two start event
    -v\t\tverbose

    (c) Matteo Corti 2006
    Report bugs to: <matteo.corti\@id.ethz.ch>

EOT

  exit;

}

__END__
