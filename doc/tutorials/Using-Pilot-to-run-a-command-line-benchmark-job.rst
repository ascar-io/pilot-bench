Using Pilot to run a command line benchmark job
===============================================

We can use Pilot to run a command line program to do a quick
benchmark, like running ``dd`` to do a quick I/O performance
benchmark. This can be done easily by using Pilot's command line
interface: the ``bench`` program.

Let's use ``dd`` as a sample. It is a common practice to use ``dd`` to
do a quick measurement of the throughput of an I/O device, such as:

.. code-block:: bash

   dd if=/dev/zero of=/mnt/testdev/io_test_file bs=1m count=100


The output on a Linux system would be like:

.. code-block:: none

   1+0 records in
   1+0 records out
   1048576 bytes (1.0 MB) copied, 0.00188192 s, 557 MB/s

On Mac OS X it would be a little different:

.. code-block:: none

   10+0 records in
   10+0 records out
   10485760 bytes transferred in 0.015194 secs (690127811 bytes/sec)

Here we have two options. Option 1 is to pass the round duration to
Pilot; Option 2 is to pass the throughput to Pilot. Both options can
give you the measured throughput along with a desired CI width. Option
1 is better because the round duration is related to the I/O
amount. Pilot will do rounds using different I/O amounts in order to
figure out the duration of warm up and cool down phases, and this can
give you a more precise result using shorter time. With Option 2 Pilot
can only get a collection of throughput samples and will not be able
to do linear analysis on the I/O amount and duration; this makes it
slower to reach a desired width of CI.

Option 1: Analyzing the Round Duration
--------------------------------------

We need to extract the duration of a benchmark round. A simple wrapper script can do that:

.. code-block:: bash

   OUTPUT_FILE=$1
   IO_COUNT=$2
   dd if=/dev/zero of=$OUTPUT_FILE bs=1m count=$IO_COUNT 2>&1 | \
       grep "bytes transferred" | \
       sed "s/^.*transferred in \([\.0-9][\.0-9]*\) secs.*/\1/g"

(this file can be found at `examples/benchmark_dd/run_dd.sh`).

Then we can use Pilot's command line program `bench` to run it:

.. code-block:: bash

   bench run_program -d 0 --wps -w 1,5000 -- ./run_dd.sh /tmp/io_benchmark %WORK_AMOUNT%


If you just build Pilot from the source code and haven't installed it
yet, the ``bench`` program should be located in ``build/cli/bench``.

The option ``run_program`` is a command that tells bench to run a
program as designated from the command line. ``-d 0`` sets the column
of the duration. bench expects the output of the workload program to
be Comma Separated Values (CSV). Since our workload only prints one
number as the duration, it is the 0th column so we pass ``-d 0`` to
bench. ``--wps`` enables the WPS analysis that can detect warm-up and
cool-down phases automatically. See the Pilot paper and manual for
more information about different analytical methods. WPS works well
and should be the default choice for such kind of command line
benchmark. ``-w 1,5000`` set the valid range of work amount for
running this workload. We are telling Pilot that it is OK to try to
run this workload using to do 1 MB to 5000 MB I/O; Pilot will start
from a small value and use many heuristics to pick the optimal work
amount, trying to finish the workload and getting a statistical valid
result fast. ``--`` indicates the end of options for Pilot and the
beginning of the workload program. All the rest of the command line
will be used to run the workload program, and you can pass whatever
parameters you need. We can see that we pass the macro
``%WORK_AMOUNT%`` to the workload as the I/O count. Pilot will
substitute that macro with the calculated work amount for running each
round of workload.

For detailed information about Pilot Bench's output, see
:ref:`Interpreting the benchmark output <interpreting-the-benchmark-output>`.

Option 2: Analyzing the Throughput Directly
-------------------------------------------

In Option 2, we are not going to use the very handy WPS method, but
are only feeding a series of throughput that are calculated by ``dd``
into Pilot. Because of the missing of round duration information,n
Pilot wouldn't be able to control the I/O amount of each round, thus
we will have to use a fix I/O amount for each round. Now it is up to
the user to decide whether the I/O amount is large enough so that each
round is dominated by the stable phase, not the warm up or cool down
phase.

The new ``run_dd_extract_tp.sh`` is only different to the previous
``run_dd.sh`` in that we now extract throughput:

.. code-block:: bash

   OUTPUT_FILE=$1
   IO_COUNT=$2

   dd if=/dev/zero of=$OUTPUT_FILE bs=1m count=$IO_COUNT 2>&1 | \
       grep "bytes transferred" | \
       sed "s/^.*secs (\([\.0-9][\.0-9]*\) bytes.*/\1/g"

(this file can be found at ``examples/benchmark_dd_no_wps/run_dd_extract_tp.sh``).

Then we can use Pilot's command line program ``bench`` to run it:

.. code-block:: bash

   bench run_program --pi "throughput,MB/s,0,1,1" -- ./run_dd_extract_tp.sh IO_SIZE


The option ``--pi`` specifies the information of the Performance Index
we want to measure. The parameter is in this format:

.. code-block:: none

   Format:     name,unit,column,type,must_satisfy:...
   name:       name of the PI, can be empty
   unit:       unit of the PI, can be empty (the name and unit are used only for display
   purpose)
   column:     the column of the PI in the csv output of the client program (0-based)
   type:       0 - ordinary value (like time, bytes, etc.), 1 - ratio (like throughput,
   speed). Setting the correct type ensures Pilot uses the correct mean
   calculation method.
   must_satisfy: 1 - if this PI's CI must satisfy the requirement of CI width; 0 (or
   missing) - record data only, no need to satisfy
   more than one PI's information can be separated by colon (:)

You can run ``./bench run_program --help`` anytime to get this help information.

You should replace ``IO_SIZE`` with something like 5000 to 10,000 MBs
at least, depending on your storage device's speed. There is no
general rule for specifying ``IO_SIZE`` because devices can be very
different, but the rule of thumb is that each round should not be
shorter than 10 seconds. You can see it is much easier to just use
Option 1 and let Pilot take care of setting the I/O work amount for
each round.
