==============
Server Options
==============

--------
SYNOPSIS
--------


.. program:: gearmand

**General options**

.. option:: -b [ --backlog ] arg (=32)

   Number of backlog connections for listen.

.. option:: --check-args

   Check command line and configuration file argments and then exit.

.. option:: -d [ --daemon ]

   Daemon, detach and run in the background.

.. option:: -f [ --file-descriptors ] arg

   Number of file descriptors to allow for the process (total connections will be slightly less). Default is max allowed for user.

.. option:: -h [ --help ]

   Print this help menu.

.. option:: -j [ --job-retries ] arg (=0)

   Number of attempts to run the job before the job server removes it. This is helpful to ensure a bad job does not crash all available workers. Default is no limit.

.. option:: -l [ --log-file ] arg

   Log file to write errors and information to.  Turning this option on also forces the first verbose level to be enabled.

.. option:: -L [ --listen ] arg

   Address the server should listen on. Default is INADDR_ANY.

.. option:: -p [ --port ] arg (=4730)

   Port the server should listen on.

.. option:: -P [ --pid-file ] arg

   File to write process ID out to.

.. option:: -r [ --protocol ] arg

   Load protocol module.

.. option:: -R [ --round-robin ]

   Assign work in round-robin order per worker connection. The default is to assign work in the order of functions added by the worker.

.. option:: -q [ --queue-type ] arg

   Persistent queue type to use.

.. option:: -t [ --threads ] arg (=4)

   Number of I/O threads to use. Default=4.

.. option:: -u [ --user ] arg

   Switch to given user after startup.

.. option:: -v [ --verbose ] arg (=v)

   Set verbosity level. Defaults to ERROR. Accepts FATAL, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO, and DEBUG.

.. option:: -V [ --version ]

   Display the version of gearmand and exit.

.. option:: -w [ --worker-wakeup ] arg (=0)

   Number of workers to wakeup for each job received. The default is to wakeup all available workers.

.. option:: --keepalive

   Enable keepalive on sockets.

.. option:: --keepalive-idle arg (=-1)

   If keepalive is enabled, set the value for TCP_KEEPIDLE for systems that support it. A value of -1 means that either the system does not support it or an error occurred when trying to retrieve the default value.

.. option:: --keepalive-interval arg (=-1)

   If keepalive is enabled, set the value for TCP_KEEPINTVL for systems that support it. A value of -1 means that either the system does not support it or an error occurred when trying to retrieve the default value.

.. option:: --keepalive-count arg (=-1)

   If keepalive is enabled, set the value for TCP_KEEPCNT for systems that support it. A value of -1 means that either the system does not support it or an error occurred when trying to retrieve the default value.

**HTTP:**

.. option:: --http-port arg (=8080)

   Port to listen on.

**sqlite**

.. option:: --libsqlite3-db arg

   Database file to use.

.. option:: --libsqlite3-table arg (=gearman_queue)

   Table to use.  

**Memcached(libmemcached)**

.. option:: --libmemcached-servers arg 

   List of Memcached servers to use.

**Drizzle/MySQL(libdrizzle)**

.. option:: --libdrizzle-host arg

   Host of server.

.. option:: --libdrizzle-port arg

   Port of server. (by default Drizzle)

.. option:: --libdrizzle-uds arg

   Unix domain socket for server.

.. option:: --libdrizzle-user arg

   User name for authentication.

.. option:: --libdrizzle-password arg

   Password for authentication.

.. option:: --libdrizzle-db arg

   Schema/Database to use.

.. option:: --libdrizzle-table arg

   Table to use.

.. option:: --libdrizzle-mysql arg

   Use MySQL protocol.

**Postgres**

.. option:: --libpq-conninfo arg

   PostgreSQL connection information string.

.. option:: --libpq-table arg (=queue)

   Table to use.

**tokyocabinet**

.. option:: --libtokyocabinet-file arg

   File name of the database. [see: man tcadb, tcadbopen() for name guidelines]

.. option:: --libtokyocabinet-optimize

   Optimize database on open. [default=true]



-----------
DESCRIPTION
-----------


Gearman provides a generic application framework to farm out work to other machines or processes that are better suited to do the work. It allows you to do work in parallel, to load balance processing, and to call functions between languages. It can be used in a variety of applications, from high-availability web sites to the transport of database replication events. In other words, it is the nervous system for how distributed processing communicates. A few strong points about Gearman:

* Open Source - It's free! (in both meanings of the word) Gearman has an active open source community that is easy to get involved with if you need help or want to contribute.

* Multi-language - There are interfaces for a number of languages, and this list is growing. You also have the option to write heterogeneous applications with clients submitting work in one language and workers performing that work in another.

* Flexible - You are not tied to any specific design pattern. You can quickly put together distributed applications using any model you choose, one of those options being Map/Reduce.

* Fast - Gearman has a simple protocol and interface with a new optimized server in C to minimize your application overhead.

* Embeddable - Since Gearman is fast and lightweight, it is great for applications of all sizes. It is also easy to introduce into existing applications with minimal overhead.

* No single point of failure - Gearman can not only help scale systems, but can do it in a fault tolerant way.

Thread Model
++++++++++++

The -t option to gearmand allows you to specify multiple I/O threads, this is enabled by default. There are currently three types of threads in the job server:

Listening and management thread - only one
I/O thread - can have many
Processing thread - only one

When no -t option is given or -t 0 is given, all of three thread types happen within a single thread. When -t 1 is given, there is a thread for listening/management and a thread for I/O and processing. When -t 2 is given, there is a thread for each type of thread above. For all -t option values above 2, more I/O threads are created.

The listening and management thread is mainly responsible for accepting new connections and assigning those connections to an I/O thread (if there are many). It also coordinates startup and shutdown within the server. This thread will have an instance of libevent for managing socket events and signals on an internal pipe. This pipe is used to wakeup the thread or to coordinate shutdown.

The I/O thread is responsible for doing the read and write system calls on the sockets and initial packet parsing. Once the packet has been parsed it it put into an asynchronous queue for the processing thread (each thread has it's own queue so there is very little contention). Each I/O thread has it's own instance of libevent for managing socket events and signals on an internal pipe like the listening thread.

The processing thread should have no system calls within it (except for the occasional brk() for more memory), and manages the various lists and hash tables used for tracking unique keys, job handles, functions, and job queues. All packets that need to be sent back to connections are put into an asynchronous queue for the I/O thread. The I/O thread will pick these up and send them back over the connected socket. All packets flow through the processing thread since it contains the information needed to process the packets. This is due to the complex nature of the various lists and hash tables. If multiple threads were modifying them the locking overhead would most likely cause worse performance than having it in a single thread (and would also complicate the code). In the future more work may be pushed to the I/O threads, and the processing thread can retain minimal functionality to manage those tables and lists. So far this has not been a significant bottleneck, a 16 core Intel machine is able to process upwards of 50k jobs per second.

For thread safety to work when UUID are generated, you must be running the uuidd daemon.

Persistent Queues
+++++++++++++++++

Inside the Gearman job server, all job queues are stored in memory. This means if a server restarts or crashes with pending jobs, they will be lost and are never run by a worker. Persistent queues were added to allow background jobs to be stored in an external durable queue so they may live between server restarts and crashes. The persistent queue is only enabled for background jobs because foreground jobs have an attached client. If a job server goes away, the client can detect this and restart the foreground job somewhere else (or report an error back to the original caller). Background jobs on the other hand have no attached client and are simply expected to be run when submitted.

The persistent queue works by calling a module callback function right before putting a new job in the internal queue for pending jobs to be run. This allows the module to store the job about to be run in some persistent way so that it can later be replayed during a restart. Once it is stored through the module, the job is put onto the active runnable queue, waking up available workers if needed. Once the job has been successfully completed by a worker, another module callback function is called to notify the module the job is done and can be removed. If a job server crashes or is restarted between these two calls for a job, the jobs are reloaded during the next job server start. When the job server starts up, it will call a replay callback function in the module to provide a list of all jobs that were not complete. This is used to populate the internal memory queue of jobs to be run. Once this replay is complete, the job server finishes its initialization and the jobs are now runnable once workers connect (the queue should be in the same state as when it crashed). These jobs are removed from the persistent queue when completed as normal. NOTE: Deleting jobs from the persistent queue storage will not remove them from the in-memory queue while the server is running.

The queues are implemented using a modular interface so it is easy to add new data stores for the persistent queue.

A persistent queue module is enabled by passing the -q or –queue-type option to gearmand. Run gearmand –help to see which queue modules are supported on your system. If you are missing options for one you would like to use, you will need to install any dependencies and then recompile the gearmand package.

Extended Protocols
------------------

The protocol plugin interface allows you to take over the packet send and receive functions, allowing you to pack the buffers as required by the protocol. The core read and write functions can (and should) be used by the protocol plugin.

HTTP
++++

This protocol plugin allows you to map HTTP requests to Gearman jobs. It only provides client job submission currently, but it may be extended to support other request types in the future. The plugin can handle both GET and POST data, the latter being used to send a workload to the job server. The URL being requested is translated into the function being called. 

For example, the request::

   POST /reverse HTTP/1.1
   Content-Length: 12

   Hello world!

Is translated into a job submission request for the function “reverse” and workload “Hello world!”. This will respond with::

   HTTP/1.0 200 OK
   X-Gearman-Job-Handle: H:lap:4
   Content-Length: 12
   Server: Gearman/0.8

   !dlrow olleH

The following headers can be passed to change the behavior of the job::

   * X-Gearman-Unique: <unique key>
   * X-Gearman-Background: true
   * X-Gearman-Priority: <high|low>
   
For example, to run a low priority background job, the following request can be sent::

   POST /reverse HTTP/1.1
   Content-Length: 12
   X-Gearman-Background: true
   X-Gearman-Priority: low

   Hello world!

The response for this request will not have any data associated with it since it was a background job::
   
   HTTP/1.0 200 OK
   X-Gearman-Job-Handle: H:lap:6
   Content-Length: 0
   Server: Gearman/0.8

The HTTP protocol should be considered experimental.

----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand>`_


--------
SEE ALSO
--------

:manpage:`gearman(1)` :manpage:`gearadmin(1)` :manpage:`libgearmand(3)` 
