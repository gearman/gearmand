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

.. option:: -S

   Enable SSL connections. Certificate paths are taken from the
   environment variables ``GEARMAND_CA_CERTIFICATE``, ``GEARMAN_CLIENT_PEM``,
   and ``GEARMAN_CLIENT_KEY`` environment variables or from the libgearman
   compile-time defaults.


**Client options**

.. option:: --ping

   Send an ECHO request to the job server and exit. No worker is required.
   Useful as a connectivity / health check. Exit status is 0 on success,
   non-zero on failure. When ``-t`` is omitted, a 2-second timeout is used.
   With ``-v``, it prints ``ping OK`` on success.

.. option:: -b

   Run jobs in the background

.. option:: -I

   Run jobs as high priority

.. option:: -L

   Run jobs as low priority

   Job assignment priority is global across registered functions: a high
   priority job for any function is assigned before normal or low priority jobs
   for other functions.

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

In ping mode (``--ping``), gearman only checks that a job server responds to
an ECHO request. No function name (``-f``) and no worker are required. This is
suitable for process supervisors and container health checks, for example::

   gearman --ping -h example.com -p 4730
   gearman --ping -h 127.0.0.1 -p 47300 -S -t 2000

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
