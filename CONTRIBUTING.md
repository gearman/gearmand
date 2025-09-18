# **Gearman welcomes contributions from the open-source community!**

### Please follow the steps given below to make your first contribution ->
You can obtain the latest source tree for Gearman by issuing the following command:

`git clone https://github.com/gearman/gearmand`

You will need to have git installed in order for this to work.

If you are building from source control (i.e. git), you should setup your environment via: \
`./bootstrap.sh autoreconf`

This will build the files you will need in order to run `./configure`

If you are working on the server, here are some handy environmental variables you can set so that you can debug `make test`:

 * GEARMAN_VALGRIND <-- runs the server under valgrind.
 * GEARMAN_MANUAL_GDB <-- runs the server under a remote version of GDB.
 * GEARMAN_LOG <-- generates a log file for you with the output of the debug for the server.

Note that GDB will pause the server while you run the GDB command.

Coding Style
------------

1. Variables during assignment should be like: `a= 12;`

2. When in doubt, use `()`. It means I clearly know that you meant for an operation to follow a specific order.

3. Cast return types void when there is a possibility of failure (don't bother with `printf`, use common sense):  `(void)some_function(...);`

4. New functions should be named `object_verb_(item_to_act_on)`. You should email the list if you are extending the API.

5. Use spaces after `while`, `for`, `do`, `if`, `else`. Don't around anything else.

6. If/else bracket style is:
   ```
   if (  )  
   {  
   }  
   else  
   {  
   }  
   ```

7. Make sure structs have a typedef with a _st suffix, enums have a _t
    suffix, and functions have a _fn suffix. For example:
    ```
    typedef struct gearman_task { ... } gearman_task_st;
    typedef enum gearman_return { ... } gearman_return_t;
    typedef gearman_return_t (gearman_complete_fn)(gearman_task_st *task);
    ```

Happy hacking :octocat:,\
  -Brian
