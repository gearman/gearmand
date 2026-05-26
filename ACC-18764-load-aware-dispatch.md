# ACC-18764: Load-Aware Job Dispatch

## Overview

This branch adds system-load propagation from Gearman workers to the gearmand server, allowing the server to prefer the least-loaded worker when deciding which sleeping worker to wake for a new job.

The change is intentionally minimal: a new binary protocol command (`SET_WORKER_LOAD`) lets workers report their 1-minute load average, and the NOOP wakeup logic in `gearman_server_job_queue()` is made load-aware when `worker_wakeup > 0`.

---

## How It Works

### Normal Gearman dispatch (before this change)

1. Workers register functions and send `PRE_SLEEP` when idle — the server marks them sleeping.
2. When a job arrives, `gearman_server_job_queue()` sends `NOOP` to sleeping workers (up to `worker_wakeup` count, or all if `worker_wakeup=0`).
3. Woken workers race to send `GRAB_JOB`; first to respond gets the job, others receive `NO_JOB` and go back to sleep.

Load-awareness is implemented at the NOOP wakeup level, not at GRAB_JOB level, because GRAB_JOB is worker-initiated and first-come-first-served — there is no way to steer it at the server.

### With this change (`worker_wakeup > 0`)

Before waking workers round-robin, the server scans all sleeping workers for the one with the lowest reported `system_load`. That worker receives `NOOP` first. The round-robin fallback loop only runs if fewer NOOPs were sent than `worker_wakeup` allows.

**Load priority rules:**
- Workers that have never sent `SET_WORKER_LOAD` have `system_load = -1.0` and are treated as lowest priority.
- Among workers with reported load, the numerically lowest value wins.
- A worker with any reported load is always preferred over one with no report.

### Worker side (PHP — `net_gearman` repo)

In `Worker::beginWork()`, before sending `pre_sleep` to each server connection:

```php
$loadAvg = sys_getloadavg();
$loadStr = sprintf('%.2f', $loadAvg[0]);
foreach ($this->conn as $socket) {
    Net_Gearman_Connection::send($socket, 'set_worker_load', array('load' => $loadStr));
    Net_Gearman_Connection::send($socket, 'pre_sleep');
}
```

The `set_worker_load` command is registered in `Connection.php` with magic number 43:

```php
'set_worker_load' => array(43, array('load'))
```

---

## Caveat: `worker_wakeup` must be non-zero

`gearmand` defaults to `--worker-wakeup=0`, which means "wake all sleeping workers". With this setting the load-aware path is **bypassed entirely** — the server sends NOOP to every sleeping worker regardless of load, because all of them will be woken anyway.

Load-aware dispatch only activates when `--worker-wakeup=N` (N ≥ 1) is passed to gearmand. With `N=1` (the most common useful value), the server wakes only the single best worker per job arrival.

**Deploy with:**
```
gearmand --worker-wakeup=1 ...
```

---

## Files Changed

### `libgearman-1.0/protocol.h`
Added new command enum value before `GEARMAN_COMMAND_MAX`:
```c
GEARMAN_COMMAND_SET_WORKER_LOAD, /* W->J: LOAD (1-min load avg as decimal string) */
```

### `libgearman/command.cc`
- Added entry to `gearmand_command_info_list[]`:
  ```c
  { "GEARMAN_SET_WORKER_LOAD", GEARMAN_COMMAND_SET_WORKER_LOAD, 1, false }
  ```
- Updated bounds checks in `gearman_strcommand()` and `gearman_enum_strcommand()` from `<= GEARMAN_COMMAND_STATUS_RES_UNIQUE` to `< GEARMAN_COMMAND_MAX` so they remain correct as commands are added.

### `libgearman-server/struct/io.h`
Added load field to `gearman_server_con_st`:
```cpp
float system_load{-1.0f}; /* -1 = not yet reported */
```

### `libgearman-server/server.cc`
Added handler for the new command (after the `SET_CLIENT_ID` case):
```c
case GEARMAN_COMMAND_SET_WORKER_LOAD:
  if (packet->arg_size[0] > 0 && packet->arg_size[0] < 32)
  {
    char buf[32];
    memcpy(buf, packet->arg[0], packet->arg_size[0]);
    buf[packet->arg_size[0]]= '\0';
    server_con->system_load= (float)atof(buf);
  }
  break;
```

### `libgearman-server/job.cc`
Replaced the NOOP wakeup loop in `gearman_server_job_queue()` with a two-phase approach:
1. When `worker_wakeup > 0`: scan all sleeping workers and pick the one with the lowest `system_load`, send it NOOP.
2. If fewer NOOPs were sent than `worker_wakeup` allows (or `worker_wakeup == 0`): fall through to the original round-robin loop for the remaining quota.

### `libgearman/add.cc`
Added `case GEARMAN_COMMAND_SET_WORKER_LOAD:` to two exhaustive switches that were compiled with `-Werror -Wswitch`, which requires every enum value to be explicitly listed.

### `libgearman-server/plugins/protocol/http/protocol.cc`
Added `case GEARMAN_COMMAND_SET_WORKER_LOAD:` to the "invalid for HTTP" group, required by `-Werror -Wswitch-enum`.

### `libgearman-server/gearmand_con.cc`
Pre-existing `-Wunused-but-set-variable` warning on `free_dcon_count`. The variable cannot be removed because the `GEARMAND_LIST__DEL` macro expands to `free_dcon_count--` by name construction. Fix: restore the variable declaration and its assignment, then suppress the warning with `(void)free_dcon_count;` after the loop.

### `tests/libgearman-1.0/protocol.cc`
Added assertions for the three commands that were missing from `check_gearman_command_t()`:
```c
ASSERT_EQ(41, int(GEARMAN_COMMAND_GET_STATUS_UNIQUE));
ASSERT_EQ(42, int(GEARMAN_COMMAND_STATUS_RES_UNIQUE));
ASSERT_EQ(43, int(GEARMAN_COMMAND_SET_WORKER_LOAD));
```

### `tests/libgearman-1.0/worker_test.cc`
Added two tests to the `worker` collection:

- **`set_worker_load_test`**: connects a raw socket, sends `CAN_DO` + `SET_WORKER_LOAD("1.50")` + `GRAB_JOB`, asserts the server responds with `NO_JOB` (not `ERROR`), confirming the command is accepted.

- **`load_aware_dispatch_test`**: registers two raw-socket workers for the same function — worker1 reports load `2.00`, worker2 reports load `0.50` — both send `PRE_SLEEP`. A background job is submitted. The test asserts that worker2 receives `NOOP` and then `JOB_ASSIGN`, while worker1 times out (no NOOP received within 500 ms).

Changed `world_create` to start gearmand with `--worker-wakeup=1` so the load-aware selection path is exercised. This is safe for all existing tests (they use single workers or don't depend on which of multiple workers is woken).

Fixed a crash in `load_aware_dispatch_test`: when `worker1->receiving()` times out, the `gearman_packet_st` is never populated, so `gearman_packet_free()` must not be called on it. Removed the erroneous free after the timeout assertion.

---

## Test Results

```
make check SPHINXBUILD=true
```

| Result | Count |
|--------|-------|
| PASS   | 25    |
| SKIP   | 9     |
| XFAIL  | 2     |
| FAIL   | 1     |

All 51 individual tests in `t/worker` pass, including both new tests:
```
worker.worker.set_worker_load       [ ok ]
worker.worker.load_aware_dispatch   [ ok ]
```

### Pre-existing upstream failure: `t/worker` exit 137

`make check` reports `t/worker` as FAIL with exit status 137 (SIGKILL). This is **not caused by our changes** — the identical failure occurs on unmodified upstream master.

The cause: the libtest framework sends `SIGKILL` to the child gearmand server processes during test teardown. The test-driver shell reports this kill as exit status 137 for the `t/worker` binary. Every individual test inside the run shows `[ ok ]`; the kill happens only after the last test completes, during process group cleanup.

To confirm individual tests all pass, run `t/worker` directly:
```sh
./t/worker
# Exit: 0
```

---

## Building on macOS (Apple Silicon / Homebrew)

The standard `./bootstrap.sh && ./configure && make` sequence requires several adjustments on macOS with Homebrew.

### Prerequisites

```sh
brew install autoconf automake libtool boost libevent openssl pkg-config
```

### Configure

Homebrew installs to `/opt/homebrew` which is not in the compiler's default search path. Pass the include/lib paths explicitly and point `--with-boost` directly at the Homebrew prefix:

```sh
CPPFLAGS="-I/opt/homebrew/include" \
LDFLAGS="-L/opt/homebrew/lib" \
./configure \
  --with-boost=/opt/homebrew/opt/boost \
  --disable-silent-rules
```

**Why `--with-boost` and not `BOOST_ROOT`?**
The `ax_boost_base.m4` macro treats `BOOST_ROOT` as a staged (not installed) boost build — it looks for `$BOOST_ROOT/stage/lib/`. Homebrew's layout doesn't match. The `--with-boost=DIR` argument correctly maps to `$DIR/include` and `$DIR/lib`.

### Man pages

The build system tries to generate man pages with `sphinx-build`. Without Sphinx installed the build fails. Pass `SPHINXBUILD=true` to no-op all sphinx invocations, then pre-create empty placeholder files for the copy step:

```sh
# Create placeholder man pages so 'cp docs/build/man/*.N man/*.N' doesn't fail
grep "dist_man_MANS" man/include.am | awk '{print $2}' | sed 's|man/||' | \
  while IFS= read -r f; do touch "docs/build/man/$f"; done

make -j$(nproc) SPHINXBUILD=true
```

The `SPHINXBUILD=true` flag must be passed to every `make` invocation (including `make check`) because the docs/Makefile evaluates it at parse time, not just when sphinx targets are built.

### Running the test suite

```sh
make check SPHINXBUILD=true
```

Optional backends (drizzle, memcached, postgres, mysql, redis, tokyocabinet) show as SKIP — this is normal; their client libraries are not installed.

To run only the worker tests:
```sh
./t/worker
```

To run only the new load-dispatch tests:
```sh
./t/worker --collection worker --wildcard "*load*"
```
