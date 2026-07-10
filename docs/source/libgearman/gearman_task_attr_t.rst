====================================
Describing Workload (gearman_work_t)
====================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_work_t

.. c:type:: gearman_job_priority_t

.. c:function:: gearman_work_t gearman_work(gearman_job_priority_t priority)

.. c:function:: gearman_work_t gearman_work_epoch(time_t epoch, gearman_job_priority_t priority)

.. c:function:: gearman_work_t gearman_work_background(gearman_job_priority_t priority)

Compile and link with -lgearman

-----------
DESCRIPTION
-----------

:c:type:`gearman_work_t` describe work for :c:func:`gearman_execute`.

:c:func:`gearman_work` creates a :c:type:`gearman_work_t` with a priority.

Job assignment priority is global across registered functions. A high priority
job for any function is assigned before normal or low priority jobs for other
functions.

:c:func:`gearman_work_epoch` creates a :c:type:`gearman_work_t` which tells :c:func:`gearman_execute` to execute the workload at the time specified by epoch.

:c:func:`gearman_work_background` creates a :c:type:`gearman_work_t` which tells :c:func:`gearman_execute` to execute the workload as a background job.

--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_execute()`
