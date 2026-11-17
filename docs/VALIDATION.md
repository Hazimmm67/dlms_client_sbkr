# Validation performed

Validated in the current Linux/WSL workspace.

- Normal CMake build: passed.
- Seven CTest checks: passed, including the new real-socket demo gateway integration.
- AddressSanitizer + UndefinedBehaviorSanitizer build: seven checks passed.
- Independent CMake parent project linking `dlms_client_sbkr::client`: built,
  loaded JSON and destroyed the handle successfully; CLI/simulator targets were
  disabled by default when embedded.
- Native DLMS TCP test: all 25 register engineering values matched the shared
  JSON; server loss produced bad quality; the same running client reconnected
  after the server restarted.
- Configuration validation: malformed JSON, duplicate IDs/OBIS, invalid ranges,
  types, intervals, serial parity and missing TCP settings rejected.
- SBKR dynamic adapter: required function loading, timeout and write arguments,
  return status propagation, optional logger and cleanup validated against an
  explicitly test-only ABI fixture, not an actual gateway.

Not validated: physical serial meter, vendor-specific IEC mode/association,
real SBKR shared library ABI, gateway provisioning or live data delivery.

Temporary SBKR demo integration also verifies persisted/read-back values, atomic
rejection of invalid batches, uint64 metadata, logger acknowledgements, gateway
restart persistence, store-write errors, missing IDs, version rejection, timeout,
and recovery on the same library client. All 25 DLMS meter values were validated
in the actual demo gateway's saved snapshot. This verifies the private demo
protocol, not production SBKR protocol compatibility.
