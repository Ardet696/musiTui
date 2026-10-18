# Tests

The test suite is run through the script at scripts/test.sh. Catch2 is pulled in automatically by CMake so you don't need to install anything extra.

To run everything just do:

```
./scripts/test.sh
```

If you want to run a specific kind of test you can pass one of these arguments: unit, integration, stress, asan, tsan, or fuzz.

The unit tests cover individual components like the ring buffer, FFT, BPM detector, config service, spectrum analyzer and path validator, plus the command dispatcher and the control socket (round trips, stale socket recovery, permissions, shutdown). The integration tests run the actual MP3 decoder against real audio files that live in tests/fixtures. The stress tests hammer the ring buffer with concurrent reads and writes to catch race conditions.

The asan and tsan modes recompile everything with sanitizers enabled and are good for catching memory errors or data races that don't show up in a normal build. The fuzz mode feeds random and mutated input into the MP3 decoder for 60 seconds to look for crashes — this one requires clang.

For a typical check after making changes you would just run ./scripts/test.sh or ./scripts/test.sh unit if you only touched something small.
