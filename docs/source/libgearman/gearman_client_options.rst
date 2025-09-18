
=============== 
Setting Options
=============== 


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_client_options_t

.. c:function:: gearman_client_options_t gearman_client_options(const gearman_client_st *client)

.. c:function:: void gearman_client_set_options(gearman_client_st *client, gearman_client_options_t options)

.. c:function:: void gearman_client_add_options(gearman_client_st *client, gearman_client_options_t options)

.. c:function:: void gearman_client_remove_options(gearman_client_st *client, gearman_client_options_t options)

.. c:function:: bool gearman_client_has_option(gearman_client_st *client, gearman_client_options_t option)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_client_options` returns the :c:type:`gearman_client_options_t` for :c:type:`gearman_client_st`. You enable options via :c:func:`gearman_client_add_options` and disable options via :c:func:`gearman_client_remove_options`.  

:c:func:`gearman_client_set_options` has been DEPRECATED.


The currently supported options are:

.. c:type:: GEARMAN_CLIENT_NON_BLOCKING

Run the cient in a non-blocking mode.

.. c:type:: GEARMAN_CLIENT_FREE_TASKS

Automatically free task objects once they are complete.

.. c:type:: GEARMAN_CLIENT_UNBUFFERED_RESULT

Allow the client to read data in chunks rather than have the library buffer
the entire data result and pass that back.

.. c:type:: GEARMAN_CLIENT_GENERATE_UNIQUE

Generate a unique id for each task created by generating a UUID.

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

