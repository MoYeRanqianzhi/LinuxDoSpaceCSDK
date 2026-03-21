# Development Guide

## Workdir

```bash
cd sdk/c
```

## Validate

Preferred direct validation:

```bash
gcc -std=c11 -Iinclude src/sdk.c examples/simple.c -o linuxdospace-c-example
./linuxdospace-c-example
```

Alternative CMake validation:

```bash
cmake -S . -B build
cmake --build build
./build/linuxdospace_c_example
```

## Release model

- Workflow file: `../../../.github/workflows/release.yml`
- Trigger: push tag `v*`
- Current release output is a source archive uploaded to GitHub Release

## Keep aligned

- `../../../include/linuxdospace/sdk.h`
- `../../../src/sdk.c`
- `../../../README.md`
- `../../../examples/simple.c`
- `../../../.github/workflows/ci.yml`
- `../../../.github/workflows/release.yml`

