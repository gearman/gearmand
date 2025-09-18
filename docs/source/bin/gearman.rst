=========================
Gearman Command Line Tool
=========================

Command line client for Gearmand

--------
SYNOPSIS
--------

.. program:: gearman

**Common options**

.. option:: -f <function>

   Function name to use for jobs (can give many)

.. option:: -h <host>

   Job server host

.. option:: -H

   Print this help menu

.. option:: -p <port>

   Gearman server port

.. option:: -t <timeout>

   Timeout in milliseconds

.. option:: -i <pidfile>

   Create a pidfile for the process

.. option:: -n

   In client mode run one job per line, in worker mode send data packet for each line

.. option:: -N

   Same as -n, but strip off the newline


**Client options**

.. option:: -b

   Run jobs in the background

.. option:: -I

   Run jobs as high priority

.. option:: -L

   Run jobs as low priority

.. option:: -P

   Prefix all output lines with functions names

.. option:: -s

   Send job without reading from standard input

.. option:: -u <unique>

   Unique key to use for job

*Worker options**

.. option:: -c <count>

   Number of jobs for worker to run before exiting

.. option:: -w

   Run in worker mode



-----------
DESCRIPTION
-----------


With gearman you can run client and worker functions from the command line. 

The environmental variable GEARMAN_SERVER can be used to specify multiple gearmand servers. Please see the c:func:'gearman_client_add_servers' for an explanation of the required syntax.


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
