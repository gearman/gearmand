==================
Gearman Admin Tool
==================

Run Administrative commands against a server.

--------
SYNOPSIS
--------

.. program:: gearadmin

.. option:: --help

   Provice help about the program.

.. option:: --create-function

   Create a function from the server.

.. option:: -h [ --host ] arg (=localhost)i

   Connect to the host

.. option:: -p [ --port ] arg (=4730)

   Port number or service to use for connection

.. option:: --drop-function

   Drop a function from the server.

.. option:: --server-version

   Fetch the version number for the server.

.. option:: --server-verbose

   Fetch the verbose setting for the server.

.. option:: --status

   Status for the server.

.. option:: --workers

   Workers for the server.

.. option:: --cancel-job

   Remove a given job from the server's queue.

.. option:: --show-unique-jobs

   Show unique IDs of jobs on the server.

.. option:: --show-jobs

   Show all jobs on the server.

.. option:: --getpid

   Get Process ID for the server.

.. option:: --priority-status

   Queued jobs status by priority.

.. option:: -S [ --ssl ]

   Enable SSL connections.


-----------
DESCRIPTION
-----------

Command line tool for manipulating gearmand servers.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`

