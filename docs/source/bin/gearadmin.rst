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


-----------
DESCRIPTION
-----------

Command line tool for manipulating gearmand servers.

----
HOME
----

To find out more information please check:
`http://gearman.info/ <http://gearman.info/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`

