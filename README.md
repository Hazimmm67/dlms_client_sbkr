# dlms_client_sbkr

A separate C99 library and command-line application derived from
`dlms_bridge_project`'s DLMS client, with a JSON point database and SBKR output.
The original bridge project is not modified. Linux/WSL is the supported build
platform for this version; the CLI uses POSIX signals, sockets and dynamic loading.

```
                      config/dtm.json
                     /               \
          DLMS meter simulator      dlms_client_sbkr
                  |                  |          |
                  +-- DLMS HDLC -----+          +-- console JSON
                                                +-- SBKR gateway adapter
```

`dtm.json` is configuration and a shared point dictionary, not a live measurement
store. Real values come from the meter. The development meter exposes the same
point list and uses `simulator_value` from the TCP demo JSON. It does not alter
the configuration file when the client polls.

For separate visible terminals and all startup commands, see
[run commands](docs/RUN_COMMANDS.md).

## Temporary SBKR demo available

A local demo library and gateway now provide a complete development flow:
**simulated meter → DLMS client → `libsbkr_demo.so` → demo gateway → saved JSON**.

```sh
cmake -S . -B build -DDLMS_SBKR_BUILD_DEMO=ON
cmake --build build -j4
python3 scripts/run_sbkr_demo.py
```

See [demo instructions](docs/DEMO_SBKR.md) for the saved readings, API support,
private demo protocol and how to replace it later. Production SBKR compatibility
remains unverified; the matching production library is still not supplied.

## Build and run

Dependencies: C compiler, CMake, pkg-config, CZMQ development package (including
ZeroMQ). On Debian/Ubuntu these are `build-essential cmake pkg-config libczmq-dev`.
Gurux and cJSON source are included, so the build does not need network access.

```sh
cd /home/hazim/github/dlms_client_sbkr
cmake -S . -B build -DDLMS_SBKR_BUILD_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dlms_client_sbkr config/dtm.json --check
```

For a real DLMS wire exchange without meter hardware, start the included server:

```sh
./build/dlms_meter_simulator config/dtm.tcp-demo.json
```

In a second terminal:

```sh
./build/dlms_client_sbkr config/dtm.tcp-demo.json --once
```

Both programs read the SAME JSON file. This local server binds only to
127.0.0.1, uses no authentication, accepts read operations, and serves 25 COSEM
Register objects. Its readings are synthetic. It is not an SBKR broker/server.
The TCP transport carries HDLC frames, matching the previous bridge; it does not
implement DLMS Wrapper framing. The simulator is a development tool, not a meter
implementation for deployment.

For an entirely offline callback demonstration (no sockets):

```sh
./build/dlms_client_sbkr config/dtm.json --simulate --once
```

That mode reports `simulated: true` and returns 1.0 per point. It refuses an SBKR
gateway setting to avoid sending synthetic values. The TCP simulator also serves
synthetic values; keep its configuration's gateway empty for local development.

## Physical meter

Edit a copy of `config/dtm.json`, set `dlms.serial_port`, client/server addresses,
and authentication to the meter's actual settings, then run:

```sh
./build/dlms_client_sbkr config/dtm.json --once
```

Omit `--once` for continuous polling, and press Ctrl+C to exit. The sample uses
9600 baud, 8N1 and a one-minute polling delay after each completed cycle.
`dlms.timeout_ms` controls reply waiting, not a hard deadline for an entire cycle;
TCP connection establishment can still follow operating-system timeouts.

The document specifies IEC 62056-21 without stating its operating mode. This
project retains the original bridge's DLMS/COSEM HDLC transport. Set
`dlms.iec_mode_e=true` only if the physical meter requires the IEC Mode E sign-on.
It is not an implementation of all IEC 62056-21 ASCII modes. Meter compatibility
and serial hardware still need a real-device check. Only COSEM Register class 3,
attribute 2 (value) and attribute 3 (scaler/unit) are read. Other object classes,
HLS authentication and encrypted associations are outside this version.

## JSON schema and point IDs

`config/dtm.original.json` preserves the supplied JSON. `config/dtm.json` enriches
it using Table 16.7 in the supplied document. The original documents are under
`docs/`. A per-point example is:

```json
{
  "description": "Active import energy aggregate",
  "obis_code": "1-1:1.25",
  "data_type": "AI",
  "casdu": 15,
  "ioa": 23,
  "tid": "T36",
  "low": -10000000000,
  "high": 10000000000,
  "sbkr_id": 983063
}
```

- Short OBIS `1-1:1.25` becomes `1.1.1.25.0.255`, following the previous bridge.
  Six-component OBIS strings are also accepted. Verify the E/F suffix against
  the meter's actual object list.
- Provisional IDs are `(CASDU << 16) | IOA`. These are explicit JSON values, not
  an SBKR standard or confirmed gateway allocation. Here IOAs fit in 16 bits.
  Replace them with the gateway's assigned point IDs when available. IDs and
  OBIS addresses must be unique within each configuration; coordinate IDs across
  multiple meter instances yourself.
- IOAs are 23, 24, 25, then 45 through 66, as in Table 16.7. CASDU is 15.
- `tid` is retained as IEC 104 metadata; the library does not implement IEC 104.
  AI values are published as `SBKR_TYPE_DOUBLE`, independently of T36 metadata.
- `low`/`high` detect invalid readings; they do not clamp values or change units.
- Optional `fallback_scaler` (-12 to 12) is used only when the meter explicitly
  denies or cannot find the scaler/unit attribute. No fallback is enabled by
  default; a failed scaler read is never silently treated as scaler zero.
- Optional `simulator_value` is an engineering value used only by the demo meter.
- `connectivity.polling_frequency` accepts `ms`, `s`, or `min`, up to 24 hours.
- `connectivity.station_id` supplies the default server address; explicit
  `dlms.server_address` takes precedence. `role`, `interface`, `device_name` and
  `protocol` are descriptive metadata. JSON cannot configure RS485 hardware
  direction control; serial device/driver setup is external.

Failed reads keep the previous numeric value with bad quality and a new UTC
observation timestamp. Consumers must check Quality. Timestamp is client-side
milliseconds since Unix epoch, not a timestamp read from the meter. Sequence
counts local observations; it is not a confirmed store sequence or write ticket.
Native read failures distinguish no access, undefined object, invalid value and
communication failure. A lost connection is reopened on the next polling cycle.
Each completed cycle passes the entire snapshot (including failures) to the
optional publisher and reports the publisher's return status.

## Production SBKR publishing: external dependency

The supplied `sbkr_client.h`, `sbkr_data.h`, and `sbkr_logger.h` are included
unchanged under `vendor/sbkr`. They declare functions but do not supply their
implementations or the SBKR data-plane wire protocol. The installed local SBKR
checkout has an older API and cannot supply these data functions.

When the matching SBKR shared library and gateway are available, configure
`sbkr.library`, `sbkr.gateway`, and optionally `sbkr.logger_endpoint`, or use:

```sh
./build/dlms_client_sbkr config/dtm.json \
  --sbkr-library /path/to/libsbkr.so --gateway tcp://gateway-host:port --once
```

The adapter requires `sbkr_data_client_new`, `sbkr_client_destroy`,
`sbkr_client_set_timeout`, and `sbkr_write_data`. It loads the actual library with
`dlopen` and rejects a library missing these symbols. Function signatures, enum
values, structures and build ABI must match the supplied headers. Configure the
same IDs and DOUBLE types in the gateway/store using that system's provisioning
procedure. No provisioning API is declared in the supplied headers.

The logger is optional, uses the supplied logger API, and submits best-effort
messages without requesting retention acknowledgements. Each client/logger
handle must be used on its owning thread.

Console mode and meter polling work without the missing SBKR shared library.
Production SBKR delivery cannot be validated until that dependency and a gateway exist;
we do not infer a wire format from the header declarations or claim console output
was delivered to a broker.

## Embed in another C/C++ application

```cmake
add_subdirectory(path/to/dlms_client_sbkr)
target_link_libraries(your_app PRIVATE dlms_client_sbkr::client)
```

Include `dlms_client_sbkr.h` and use this lifecycle:

```c
char error[256];
dlms_client_sbkr_t *client = dlms_client_sbkr_create("dtm.json", NULL,
                                                     error, sizeof(error));
if (!client) {
    fprintf(stderr, "%s\n", error);
    return 1;
}
dlms_sbkr_poll_result_t result;
int status = dlms_client_sbkr_poll(client, &result);
/* Inspect status, result, and dlms_client_sbkr_samples(client). */
dlms_client_sbkr_destroy(&client);
```

`create` only parses configuration. `poll` connects as needed. The library never
owns a scheduler or sleeps between cycles: the embedding application decides
when to call it. Return values: 0 means all reads/publication succeeded, 1 means
read failures, -1 means invalid state/publication failure. Inspect per-point
Quality as well. Public sample/point pointers are borrowed and expire on destroy;
samples change on the next poll. Serialize all calls on a handle.

Use `dlms_sbkr_callbacks_t.read` to inject your existing meter reader (return scaled
engineering doubles). Use its `publish` callback to integrate your own publisher,
or use `dlms_sbkr_gateway_open` and `dlms_sbkr_gateway_publish` from
`dlms_sbkr_gateway.h`. Keep the gateway alive until after destroying its client.
Each handle owns its configuration/session; no Modbus server, register map, UI,
or original bridge executable is required. Transport helper/Gurux symbols are
linked internally; applications already using Gurux should use one consistent
Gurux source/version and review callback symbol ownership.

## Verification

CTest covers configuration rejection, 25-point mapping, double precision,
per-point failures, NaN/range quality, recovery, publication errors, independent
handles and missing SBKR dependency. The TCP test performs real Gurux association
and register reads using the shared JSON, then stops/restarts the demo meter to
check reconnect on the same running client. These are development simulations;
physical-meter and live-SBKR integration remain unverified.
The dynamic adapter also has a test-only ABI fixture, which does not implement
an SBKR wire protocol. Both the normal and address/undefined sanitizer builds
passed the original six checks; an independent parent project built the library too.

See `docs/DESIGN.md` for source findings and boundaries, and
`THIRD_PARTY_NOTICES.md` for included dependency origins/licenses.
