# LJRV - LuaJIT RISC-V 64 Port

LuaJIT is a Just-In-Time (JIT) compiler for the Lua programming language,
RISC-V is a free and open ISA enabling a new era of processor innovation.

Find latest LJRV release at [IgnotaYun/LuaJIT,v2.1-riscv64](https://github.com/IgnotaYun/LuaJIT/tree/v2.1-riscv64).
Development branch is avaliable at [IgnotaYun/LuaJIT,riscv](https://github.com/IgnotaYun/LuaJIT/tree/riscv).

**This is the release branch of LJRV, containing patchset based on dev branch commit [63170372db36 ("riscv(interp): Backport some v3.0 syntax extensions.")](https://github.com/IgnotaYun/LuaJIT/commit/63170372db368ca9c970012056c76b9e09b64512), based off LuaJIT v2.1 rolling [1edc3e52b67e ("x64/LJ_GC64: Fix XLOAD fusion.")](https://github.com/LuaJIT/LuaJIT/commit/1edc3e52b67eaf6ce5f809be8e17d6862594b8bc)**

## Introduction

LJRV is a ongoing porting project of LuaJIT to the RISC-V 64-bit architecture by ISRC, ISCAS.
The ultimate goal is to provide a RISC-V 64 LuaJIT implementation and have it upstreamed to the official LuaJIT repository.

## Building and Packaging

LJRV is built and packaged in the same way as LuaJIT, requires a recent toolchain based on GCC or Clang with RISC-V 64 support and GNU Make.
For building and installation instructions, please refer to the [LuaJIT README](https://luajit.org/install.html).

For distro maintainers and packagers, we encourage one to follow our branch and patchset, as we would keep it up-to-date with the latest LuaJIT upstream. Cherry-picking and backporting is **strongly** discouraged, ["no matter how self-standing individual changes look (because they often are not)"](https://luajit.org/download.html).

## Progress

- [x] Interpreter Runtime
- [x] JIT Compiler

LJRV is still considered of beta quality, take it with a grain of salt.

FFI struct passing is known to be partially broken, please report any issue you encounter.

## Bug Report

Please report bugs to [Issues](https://github.com/ruyisdk/LuaJIT/issues).

## Copyright

LuaJIT is Copyright (C) 2005-2026 Mike Pall.
LuaJIT is free software, released under the MIT license.
See full Copyright Notice in the COPYRIGHT file or in luajit.h.

LJRV is Copyright (C) 2022-2026 ISRC, ISCAS. Contributed by gns.
LJRV is free software, released under the MIT license.
LJRV is part of openRuyi.
