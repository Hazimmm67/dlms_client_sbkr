# Source study and design decisions

A temporary library and local gateway have since been added; see
[DEMO_SBKR.md](DEMO_SBKR.md). Their private demo protocol does not establish
production SBKR wire compatibility.

## Inputs examined

- Original `dlms_bridge_project/include/dlms_client.h` and its implementation,
  configuration, meter profiles, connection/communication modules and callbacks.
- Supplied `dtm.json` (25 AI points).
- Supplied `sbkr_client.h`, `sbkr_data.h`, `sbkr_logger.h`.
- Entire Word document, particularly serial settings in section 15 and DTM
  point mapping in Table 16.7.
- Local SBKR implementation: it has broker request/reply functions but lacks
  the data-plane functions declared in the supplied newer header.

The three SBKR headers were found directly in Downloads, rather than the
`Downloads/sbkr/` paths originally mentioned. Their copies here are unchanged.
The Word document is reference material; its network addresses describe an
installation and are not instructions to configure the host or contact services.
No host addresses, VPN, network interfaces, or production meters were changed.

## Mapping

The JSON OBIS points match the document's 25 DTM rows in order. The document
supplies CASDU 15, IOA 23/24/25 then 45..66, T36, and allowed engineering ranges.
Its data-frequency column says `Sp`, whereas the JSON specifies `1min` polling.
We preserve one-minute polling from JSON; publication occurs once per completed
cycle. No change-only/deadband behavior is inferred from `Sp`.

The source JSON supplies neither physical serial device name, DLMS client SAP,
authentication details, complete six-byte OBIS names, nor SBKR point IDs. The
new example adds explicit runtime fields. Six-byte OBIS expansion follows the
original bridge's `{A,B,C,D,0,255}` map. Client address 16, server address 1,
no authentication, and direct HDLC are editable sample values. They are not
assertions about the physical meter's required settings.

Following the user's confirmation that IDs are not assigned, the example uses
provisional `(15 << 16) | IOA` IDs. This convention is local to this sample.
Real gateway IDs and schemas must be coordinated with the SBKR deployment.

## Separation of responsibilities

- `include/dlms_client_sbkr.h`: reusable instance-based lifecycle, callbacks,
  point metadata, snapshots and poll results.
- `src/config.c`: bounded JSON loading and validation.
- `src/transport.c`: Gurux association, scaler/value reads and session lifecycle.
- `src/dlms_client_sbkr.c`: snapshot quality, observation timestamps, local
  sequences, callbacks and memory ownership.
- `src/gateway.c`: dynamically loads the newer SBKR API; optional async logger.
- `src/main.c`: scheduling, signals, console output and command-line arguments.
- `src/meter_simulator.c`: local development meter exposing the same JSON points
  as COSEM Registers over HDLC/TCP, with explicit synthetic values.

Unlike the original bridge's global float structure, points are dynamically
allocated per handle and preserve engineering values as doubles. Native reads
apply the meter's scaler exactly once. The new library has no Modbus translation
or status-register dependencies. The runtime does not rewrite the shared JSON.

The previous `com_close()` also cleared Gurux settings. A reconnect cannot reuse
that cleared state without rebuilding addresses, authentication, interface,
buffers and objects. The new transport explicitly cleans up and rebuilds this
state. The copied transport's fractional TCP receive timeout now preserves
milliseconds instead of rounding them down to whole seconds.

## Boundaries and remaining integration work

The supplied SBKR headers contain no wire framing, gateway implementation,
store schema provisioning, or broker endpoint. Reimplementing those from function
names would be guessing. This project calls the declared functions through the
runtime adapter and provides a clear missing-library error. A compatible library
with its transitive dependencies, real point provisioning and gateway endpoint
are necessary for live SBKR delivery.

The document lists MQTT deployment addresses, while the supplied client API
uses CZMQ and a dedicated gateway data client. These are distinct interface
surfaces. No MQTT topic, port, payload schema or bridge is invented from those
addresses. Only the explicitly configured SBKR gateway is used.

Physical serial hardware, RS485 adapter behavior, IEC mode, object classes and
access rights must be checked against the real meter. Tests validate real DLMS
framing and association with the bundled simulator, not physical meter behavior.
JSON remains a shared configuration database; a durable measurement/history
store belongs to the actual SBKR gateway/store. No embedded persistence backend
is invented here.
