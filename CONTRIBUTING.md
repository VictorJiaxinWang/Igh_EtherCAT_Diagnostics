# Contributing

Contributions should keep the diagnostic process independent from the EtherCAT real-time control loop.

Before submitting a change:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
cd build && ctest --output-on-failure
```

Add or update tests for behavior changes. Keep public data formats backward compatible when possible; document schema changes in `docs/WEB_DATA_SCHEMA.md`. Do not commit build directories, runtime logs, device dumps, credentials, or machine-specific files.

