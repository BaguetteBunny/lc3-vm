# LC-3 Virtual Machine

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](license)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.cppreference.com/w/c)

A simple LC-3 virtual machine written in C, supporting basic instruction execution, traps, and input recording/replay. Heavily inspired from [J. Miner's Tutorial](https://www.jmeiners.com/lc3-vm/).

---

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Usage](#usage)

---

## Overview

The LC-3 VM is an educational project that simulates the LC-3 architecture. It includes:

- Execution of LC-3 binary programs (`.obj` files)
- Optional input recording and replay
- Keyboard input handling
- Executes 16-bit LC-3 instructions
- Supports **all 16 LC-3 opcodes**
- Support on *virtually* all OS; Windows, macOS, and Linux

Basic programs are given in the [programs](programs/) folder. Instructions on usage down below.

---

## Prerequisites

- C compiler (GCC, Clang, or MSVC)
- Make (optional)
- Terminal or PowerShell

---

## Usage

```bash
# Run and record keyboard input
./lc3-vm path/to/program.obj --record

# Run and replay previously recorded input
./lc3-vm path/to/program.obj --replay