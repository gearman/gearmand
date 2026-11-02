# Threat model

## What this project does and where untrusted input enters
gearmand is a job server. Clients submit jobs and workers fetch them over TCP
(default port 4730), optionally with TLS (`--enable-ssl`). `libgearman` is the
client/worker library that applications embed.

gearmand has no authentication: anyone who can connect may submit jobs, register
as a worker and use the text admin protocol. Deployments are expected to restrict
access at the network level. Because of this, being able to do these things is
not a vulnerability. What matters is what a connected peer can do beyond them.

Untrusted input:
- The binary protocol (`PROTOCOL`, `libgearman-server/plugins/protocol/gear/`,
  `libgearman-server/packet.cc`, `io.cc`, `connection.cc`) from any client or worker.
- The text admin protocol (`libgearman-server/text.cc`): `status`, `workers`,
  `maxqueue`, `show jobs`, `show unique jobs`, `cancel job`, `create function`,
  `drop function`, …
- The HTTP protocol plugin (`libgearman-server/plugins/protocol/http/`), enabled with
  `-r http`.
- On the library side, responses from a gearmand server as parsed by `libgearman`.
  A malicious or compromised server corrupting the memory of an embedding
  application is in scope.

Trusted: the command line, the configuration file, the persistent queue backends'
databases (sqlite3, tokyocabinet, redis, memcached, MySQL, PostgreSQL) and the TLS
certificates.

## Components that matter most / least
- Most: packet parsing and buffer handling in `libgearman-server/`, the text and
  HTTP protocols, job/unique-ID/function-name handling, and the TLS code paths.
- Also in scope: response parsing in `libgearman/`, and SQL built from job data in
  the queue plugins (`libgearman-server/plugins/queue/`).
- Less: the command line tools in `bin/` (`gearman`, `gearadmin`).
- Out of scope: `libtest/`, `libhostile/` (test-only fault injection), `tests/`,
  `benchmark/`, `examples/`, `docker/`, and the bundled `libhashkit/` unless it
  is reachable from the network.

## How to exercise it
- Start a server: `/src/gearmand/gearmand --port 4730 --verbose DEBUG --log-file stderr`
  (use `/src-asan/gearmand/gearmand` with ASan). Add `-r http` for the HTTP
  protocol (port 8080 by default, `--http-port` to change it).
- Persistent queues: `-q libsqlite3 --libsqlite3-db=/tmp/q.db`, `-q libtokyocabinet --libtokyocabinet-file=/tmp/q.tch`,
  `-q redis` (start `redis-server` first), `-q libmemcached` (start `memcached`).
- Talk to it with `/src/bin/gearman` and `/src/bin/gearadmin`, or send raw bytes
  with Python. Admin commands are plain text lines on the same port.
- The test suite is `make test` in `/src`. Individual tests are built in `t/`,
  e.g. `./t/protocol`, `./t/gearmand`, `./t/client`. Some tests are timing
  sensitive; ignore failures that also happen without your change.

## How you rate severity
- Critical: remote code execution, or memory corruption that is plausibly
  exploitable for it, triggered by a peer over the gearmand protocols.
- High: other memory-safety bugs (out-of-bounds write, use-after-free, double
  free) reachable over the network; reading other clients' job data through a
  memory disclosure; SQL injection into a queue backend through job data.
- Medium: a crash, assertion, infinite loop or deadlock that a single peer can
  trigger with a small number of requests; bounded out-of-bounds reads.
- Low: memory or CPU exhaustion that needs sustained traffic; bugs reachable only
  from a malicious server (in `libgearman`) without memory corruption.

Reproduce every finding against the normal build in `/src`. An ASan report alone
does not set severity, but a reliable crash in the normal build does.

## Anything to leave alone
- Missing authentication or authorization, and anything that follows from it
  (cancelling other clients' jobs, dropping functions or changing `maxqueue`
  via the admin protocol, worker impersonation). This is by design.
- Lack of TLS by default, and cleartext traffic.
- Resource exhaustion that requires opening many connections, or queueing many
  jobs, since an unlimited queue is the default (`maxqueue` limits it).
- Issues that need a malicious configuration file, command line, or queue
  backend database.
- Bugs only in `libtest/`, `libhostile/` or the test suite.

## Reports and patches
- Target the `master` branch. Patches should follow the existing C++ style and
  include a regression test under `tests/` where practical.
- A reproducer as a short Python script that speaks the protocol over a socket,
  or a `gearman`/`gearadmin` command sequence, is preferred.

## Disclosure
Findings go privately to the contacts in `project.yaml`. Do not put suspected
vulnerabilities, reproducers, or unreviewed findings in public issues or PRs.
