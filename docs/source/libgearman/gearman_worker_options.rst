==============
Worker Options
==============


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_worker_options_t

.. c:function:: gearman_worker_options_t gearman_worker_options(const gearman_worker_st *worker)

.. c:function:: void gearman_worker_add_options(gearman_worker_st *worker, gearman_worker_options_t options)

.. c:function:: void gearman_worker_remove_options(gearman_worker_st *worker, gearman_worker_options_t options)

.. c:function:: void gearman_worker_set_options(gearman_worker_st *worker, gearman_worker_options_t options)

.. deprecated:: 0.21


-----------
DESCRIPTION
-----------

:c:func:`gearman_worker_options` returns the :c:type:`gearman_worker_options_t` for :c:type:`gearman_worker_st`. You enable options via :c:func:`gearman_worker_add_options` and disable options via :c:func:`gearman_worker_remove_options`.  



The currently supported options are:

.. c:type: GEARMAN_WORKER_NON_BLOCKING

Enable non-block IO for the worker.

.. c:type:: GEARMAN_WORKER_GRAB_UNIQ

Only grab jobs that have been assigned unique values. This is useful for workers who only want to worker with background jobs.

.. c:type:: GEARMAN_WORKER_TIMEOUT_RETURN

Has a return timeout been set for the worker.

------------
RETURN VALUE
------------

Various

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso::

  :manpage:`gearmand(8)` :manpage:`libgearman(3)`

