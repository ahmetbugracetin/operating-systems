# HW2 – Crossroad

A concurrent intersection simulation written in C++ for the Operating Systems course (CENG350).

## Overview

Simulates cars arriving at a crossroad with horizontal and vertical lanes. Cars must cross the intersection without collision, following priority rules and FIFO ordering. Synchronization is implemented using a Monitor with condition variables.

## Features

- Monitor-based synchronization (`__synchronized__` macro)
- Priority-aware scheduling: higher-priority lanes proceed first
- FIFO tie-breaking: when priorities are equal, earlier arrival time wins
- Per-lane condition variables for fine-grained signaling
- Multithreaded: each car runs in its own pthread

## Build

```bash
make
```

## Key Concepts

- `pthread` for multithreading
- Monitor pattern with mutual exclusion
- Condition variables (`wait` / `signal`) for blocking and waking threads
- Priority queues and arrival-time tracking for deadlock-free scheduling
