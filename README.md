# job-scheduler

Fork-based job scheduler in C++20: process pool, dependencies between jobs, and simple scheduling (at a time, every N seconds, cron-ish). Exposed over HTTP or a small interactive CLI.

![Architecture diagram](/docs/architecture.png)

## Build

You need CMake ≥ 3.22, a C++20 compiler, Boost (`program_options`, `system`, `log`, `log_setup`), and Git (CMake pulls GoogleTest and nlohmann/json). The build runs **clang-tidy** on compile; install it or configure without it if you prefer:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# optional: add -DCMAKE_CXX_CLANG_TIDY="" to skip clang-tidy
cmake --build build -j"$(nproc)"
```

Binaries land in `build/`: `job_scheduler_http`, `job_scheduler_cli`, plus `nlp_example` and `temperature_example` for the demos. Tests: `ctest --test-dir build` or `./build/job_scheduler_tests`. Docs: `cmake --build build --target doc` (needs Doxygen) → `build/docs/html/index.html`.

## Run

**Default HTTP** (`./build/job_scheduler_http`) — jobs `echo`, `sleep`, `fail`. List jobs with `GET /jobs`, submit with `POST /jobs` (JSON). Quick check:

```bash
curl -s -X POST http://localhost:8080/jobs \
  -H 'Content-Type: application/json' \
  -d '{"kind":"run","name":"echo","body":{"args":{"msg":"hello"}}}'

curl -s http://localhost:8080/jobs
```

Exact request shapes live in `src/api/http_controller.cpp`.

**CLI** — `./build/job_scheduler_cli`, type `help` at the prompt; same three job types as the default HTTP app.

**Demos** — the scripts under `examples/` expect the matching server: `./build/nlp_example` then `examples/nlp_processing/demo.sh`, or `./build/temperature_example` then `examples/temperature_sensor/demo.sh`. They assume `http://localhost:8080` unless you set `BASE_URL`.
