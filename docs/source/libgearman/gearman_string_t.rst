========================== 
Strings (gearman_string_t)
==========================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_string_t

.. c:macro:: gearman_size(string)

   Returns the size of the string.

.. c:macro:: gearman_c_str(string)

   Returns the C string representation.

Compile and link with -lgearman.

-----------
DESCRIPTION
-----------

The :c:type:`gearman_string_t` is a simple type representing a "string".
Once created :c:type:`gearman_string_t` are not mutable once created. They
only point to the strings that define them and require no deallocation.
   
--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`



