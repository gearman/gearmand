=====================================
Welcome to the Gearman documentation!
=====================================

Gearman provides a generic application framework to farm out work to other
machines or processes that are better suited to do the work. It allows you
to do work in parallel, to load balance processing, and to call functions
between languages. It can be used in a variety of applications, from
high-availability web sites, to the transport of database replication events.


------------
Introduction
------------
.. toctree::
   :maxdepth: 1
   
   license
   install
   how_to_report_a_bug
   glossary
   architecture

------
Server
------
.. toctree::
   :maxdepth: 1

   gearmand
   gearmand/queues
   gearmand/ssl

------------------
Command Line Tools
------------------

.. toctree::
   :maxdepth: 1

   bin/gearman
   bin/gearadmin


--------------------
C/C++ Client Library
--------------------

.. toctree::
   :maxdepth: 1

   libgearman/index

--------
Protocol
--------

.. toctree::
   :maxdepth: 1

   protocol/gear
   protocol/text

-------------------
Version Information
-------------------

.. toctree::
   :maxdepth: 1

   changes


.. toctree::
   :hidden:

   libgearman

------------------
Indices and Tables
------------------

* :ref:`genindex`
* :ref:`search`
