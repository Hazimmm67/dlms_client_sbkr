# Temporary SBKR demo

This implements the subset of the supplied SBKR C API used by the meter bridge.
It includes a shared library (`build/libsbkr_demo.so`) and an actual local gateway
process (`build/sbkr_demo_gateway`). The library's private JSON protocol is named
`sbkr-demo/1`. It has not been reverse-engineered from, or verified against, the
production SBKR protocol. Use the real library and real gateway together later.

## One-command run

From this project directory, build and start:

```sh
cmake -S . -B build -DDLMS_SBKR_BUILD_DEMO=ON
cmake --build build -j4
python3 scripts/run_sbkr_demo.py
```

The runner starts the meter on 127.0.0.1:14061, the demo SBKR gateway on
127.0.0.1:15555, and the DLMS client publishing once per second. All three
processes use `.run/dtm.sbkr-demo.json`. It is generated from the bundled TCP
meter sample, with the actual shared library path filled in.

Outputs:

- `.run/sbkr-store.json`: gateway's current saved points, quality, timestamps,
  local observation sequences, successful write-batch count, and latest 100 logs.
- `.run/latest.json`: latest client observation, labeled with `demo: true`.
- `.run/demo-status.json`: child PIDs, addresses and most recent client status.
- `.run/client.log`, `gateway.log`, `meter.log`: logs capped at 1 MiB each plus
  two rotated backups per process.

Ctrl+C stops the runner and all three children. If started from another terminal,
stop it using the supervisor PID in `.run/demo-status.json`:

```sh
python3 -c 'import json,os,signal; s=json.load(open(".run/demo-status.json")); os.kill(s["supervisor_pid"],signal.SIGTERM)'
```

The saved store is preserved for the next run. Two runners cannot share an output
directory, and two gateways cannot write the same store. Ports can be changed
using `--meter-port` and `--gateway-port`; use `--output-dir` for a separate run.

The displayed meter values are synthetic. A gateway status of zero means the
local demo gateway acknowledged the operation; it says nothing about delivery to
a production SBKR deployment.

## Separate processes

From the project root, use three terminals:

```sh
./build/dlms_meter_simulator config/dtm.sbkr-demo.json
```

```sh
mkdir -p .run
./build/sbkr_demo_gateway tcp://127.0.0.1:15555 .run/sbkr-store.json
```

```sh
./build/dlms_client_sbkr config/dtm.sbkr-demo.json
```

Stop the one-command runner before using those same ports. The relative library
path in the example configuration is resolved against the working directory.
The runner uses an absolute path instead.

## Implemented API

The library exports the supplied header's symbols and supports:

- `sbkr_data_client_new`, destruction, positive timeouts and gateway reconnection.
- `sbkr_write_data`, `sbkr_read_data`, and their pointer-array variants.
- `sbkr_logger_open`, `sbkr_logger_log_async`, `sbkr_logger_receive`, and close.

Data operations support `SBKR_TYPE_DOUBLE` only, matching the DLMS bridge output.
Other value types return `SBKR_STATUS_NOT_SUPPORTED`. Up to 4096 distinct points
are stored; each data request contains 1..4096 points/IDs. Writes reject duplicate
IDs within a batch, invalid quality, nonfinite values and ID zero. The gateway
validates the complete update before committing it. Reads return NOT_FOUND when
any requested ID is absent and leave the caller's array unchanged.

Writes are acknowledged only after serializing the merged latest-value snapshot,
flushing and fsync'ing a temporary file, then atomically replacing the snapshot.
File errors are returned to the publisher. Existing malformed stores cause
startup to fail rather than being silently overwritten. The store contains the
latest observation per ID, not a time-series history. A gateway restart reloads
that store; the client's REQ socket is recreated after a timeout so later
operations can recover without reusing the wrong outstanding reply.

The logger queues one request without waiting for its reply. It allows one
outstanding message per handle; backpressure returns -1. When a ticket is
requested, `sbkr_logger_receive` reports retention/rejection after the gateway's
reply. Without a ticket, the next send drains the previous reply if available.
Best-effort messages can be lost on immediate close. This demo stores log text
in the same snapshot and retains the latest 100 messages.

Historical synchronized reads, subscriptions/events, control-plane broker
requests, and generic socket-option access fail explicitly as unsupported.
Handles are used by one thread at a time. Data writes do not assign new sequence
numbers: the DLMS reader's observation sequence is preserved.

## Demo protocol

Only `tcp://127.0.0.1:PORT` endpoints are accepted. There is no authentication,
TLS, production broker discovery, MQTT, IEC 104 provisioning or access control.
No additional runtime dependencies are introduced beyond the project's CZMQ and
cJSON dependencies. The gateway uses ZeroMQ REP, and each library handle uses its
own REQ socket. One JSON frame carries a versioned request/reply.

Operations are `write` with `points`, `read` with `ids`, and `log` with source,
message and level. All replies contain `protocol: "sbkr-demo/1"` and the canonical
numeric `sbkr_errorcode_t` status. Point fields are `id`, `type: "double"`,
`quality`, `value`, `sequence`, and `time_ms`. The last two are decimal strings to
preserve unsigned 64-bit precision. This format belongs only to this demo.

## Validation

The integration test runs a real local gateway and exercises API write/readback,
pointer arrays, double precision and full unsigned 64-bit metadata, logger ACKs,
unsupported types, malformed/version-mismatched requests, partial-write rollback,
missing IDs, storage failure, corruption refusal, exclusive store ownership,
persistence across restarts, timeout and recovery of the same client handle.
It also publishes all 25 points from the DLMS TCP simulator through the dynamic
library adapter and checks every value saved by the gateway.

## Replacing it

Change `sbkr.library`, `sbkr.gateway`, and `sbkr.logger_endpoint` to the real
implementation and endpoints. Confirm that the real library matches the supplied
header ABI and provision the correct point IDs/types in the production store.
The DLMS client API and polling code do not need to change. Turn the demo build
off with `-DDLMS_SBKR_BUILD_DEMO=OFF` when it is no longer needed.
