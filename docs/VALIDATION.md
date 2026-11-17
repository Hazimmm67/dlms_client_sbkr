# Validation performed

Validated in the current Linux/WSL workspace.

- Normal CMake build: passed.
- Eight CTest checks: passed, including the new real-socket demo gateway integration.
- AddressSanitizer + UndefinedBehaviorSanitizer build: eight checks passed.
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

## 2026-11-17 SBKR verification

Fresh normal and address/undefined sanitizer builds each passed 8/8 tests. A
continuous run verified 100 healthy polls (2,500 point observations), gateway
restart persistence and timeout recovery on the same running client, and meter
loss/reassociation with bad-quality then GOOD readings saved by the gateway.

One launcher issue was corrected: a blank transport diagnostic line was being
treated as EOF, stopping monitor updates. The new `demo_runner_recovery` test
freezes/resumes the meter and checks monitoring recovery and process cleanup.
The launcher also accepts `--build-dir` to select the binaries under test.

The detailed local report and logs are under
`.run/sbkr-verification-2026-11-17/REPORT.md` (runtime artifacts excluded from Git).
Production SBKR wire compatibility and physical serial hardware remain untested.
