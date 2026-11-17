# Run the DLMS + temporary SBKR demo

Use Ubuntu/WSL terminals. Start in this directory in each terminal:

```sh
cd /home/hazim/github/dlms_client_sbkr
```

The services may already be running in visible windows. Do not start another
copy on the same ports. Press Ctrl+C in each service window to stop that copy.
The wrappers leave an interactive Ubuntu shell open afterward.

## Build (when services are stopped)

```sh
cmake -S . -B build -DDLMS_SBKR_BUILD_DEMO=ON -DDLMS_SBKR_BUILD_TESTS=ON
cmake --build build -j4
```

## Terminal 1: simulated meter

```sh
./build/dlms_meter_simulator config/dtm.sbkr-demo.json
```

This serves the 25 synthetic readings over DLMS HDLC/TCP at 127.0.0.1:14061.
It normally prints its startup line and waits for requests.

## Terminal 2: temporary SBKR gateway

```sh
mkdir -p .run
./build/sbkr_demo_gateway tcp://127.0.0.1:15555 .run/sbkr-store.json
```

This receives demo-library requests and saves acknowledged values in the JSON
snapshot. It normally prints its startup line and waits for requests.

## Terminal 3: continuously publishing DLMS SBKR client

```sh
./build/dlms_client_sbkr config/dtm.sbkr-demo.json
```

Expected output: `25 good, 0 failed, SBKR status 0`, plus a JSON snapshot every
second. This configuration loads `./build/libsbkr_demo.so`. A zero SBKR status
means the local demo gateway acknowledged that request.

To explicitly specify the demo library and endpoint:

```sh
./build/dlms_client_sbkr config/dtm.sbkr-demo.json \
  --sbkr-library ./build/libsbkr_demo.so \
  --gateway tcp://127.0.0.1:15555
```

To read and publish one cycle, use this instead of the continuous client:

```sh
./build/dlms_client_sbkr config/dtm.sbkr-demo.json --once
```

## Terminal 4: live table of saved gateway readings

```sh
python3 scripts/watch_sbkr_demo.py
```

The table refreshes when the gateway store changes. Ctrl+C closes this monitor
without stopping the services. For one snapshot:

```sh
python3 scripts/watch_sbkr_demo.py --once
```

## One terminal: start and manage all services

Stop the separate meter, gateway and client first, then:

```sh
python3 scripts/run_sbkr_demo.py
```

This supervisor writes bounded process logs and status under `.run/`; Ctrl+C
stops all three children. It is an alternative to the three separate service
terminals. Do not run both arrangements on the same ports.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

The tests select separate temporary ports and stores. The meter values and SBKR
wire protocol used here are for demonstration. Production compatibility is not
established by these tests.
