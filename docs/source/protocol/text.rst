======================
Gearmand TEXT Protocol 
======================

Gearmand supports a very simple administrative protocol that is enabled on the same port used by GEAR. You can make use of it via the gearadmin tool.

The following list represents the current list of commands supported. You can issue any of these by telneting to the port and typing them in.

.. describe:: workers

   Return the status of all attached workers.

.. describe:: status

   Return the status of all current jobs.

.. describe:: cancel job

   Cancel a job that has been queued.

.. describe:: show jobs

   Show all current job ids.

.. describe:: show unique jobs

   List all of the unique job ids that the server currently is processesing or waiting to process.

.. describe:: create

   Create a function (i.e. queue).

.. describe:: drop

   Drop a function (i.e. queue).

.. describe:: maxqueue

   Set maxqueue

.. describe:: getpid

   Return the process id of the server.

.. describe:: verbose

   Change the verbose level of the server.

.. describe:: version
   
   Version number of server.
