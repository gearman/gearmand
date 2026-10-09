=====================================
Selecting a Server by Job Unique
=====================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: void gearman_client_set_server_selection_by_unique(gearman_client_st *client, bool enable)

.. c:function:: bool gearman_client_server_selection_by_unique(const gearman_client_st *client)

-----------
DESCRIPTION
-----------

By default, when a client with more than one server added via
:c:func:`gearman_client_add_server` submits a job, it is sent to the first
idle server in the order the servers were added -- the job's unique
identifier plays no part in the choice.

:c:func:`gearman_client_set_server_selection_by_unique` opts a client into
choosing the destination server by hashing the job's unique instead, using a
ketama consistent-hash ring built from the client's current server list. The
practical effect is that submissions sharing the same unique (from this
client, or from any other client configured with the same server list and
this option enabled) consistently land on the same server, and adding or
removing a server only reshuffles the jobs that server itself was
responsible for.

This only affects job submission (:c:func:`gearman_client_do`,
:c:func:`gearman_client_do_background`, and their variants, plus
:c:func:`gearman_client_add_task` and friends); commands such as status
lookups are unaffected. A job submitted with an empty unique falls back to
the default selection, since there is no meaningful value to hash.

If the hash-selected server cannot be reached, the client fails over to the
next server in ring order rather than failing the submission outright, the
same way the default selection falls through the server list on a
connection failure.

This option is disabled by default and is not one of the bit flags
controlled by :c:func:`gearman_client_add_options` /
:c:func:`gearman_client_remove_options`; it is copied by
:c:func:`gearman_client_clone` like the client's other persistent settings.

:c:func:`gearman_client_server_selection_by_unique` returns whether the
option is currently enabled.

------------
RETURN VALUE
------------

:c:func:`gearman_client_server_selection_by_unique` returns a ``bool``; it
returns ``false`` for a ``NULL`` client.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso::

   :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_client_st` :manpage:`gearman_client_add_server`
