# HW1 – Merger

A multi-process pipeline system written in C for the Operating Systems course (CENG350).

## Overview

`merger` reads a specification from stdin that defines chains of operations (sort, filter, unique) to apply on lines of a text file. Each chain runs as a separate process pipeline connected via Unix sockets, and the results are merged in order.

## Features

- Recursive merger trees (a chain can itself be a sub-merger)
- Supported operators: `sort`, `filter`, `unique`
- Supported column types: `text`, `num`, `date`
- Inter-process communication via `socketpair` and `fork`/`exec`
- Feeder process distributes input lines to each chain concurrently

## Build

```bash
make
```

## Usage

```bash
./merger < spec.txt
```

The specification format defines the number of chains, line ranges, and the operator pipeline for each chain.

## Key Concepts

- `fork` / `exec` for process creation
- `socketpair` for bidirectional IPC
- `waitpid` for process synchronization
- Recursive spec serialization via `dprintf`
