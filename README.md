# Multithreaded CPU Simulator (Blocked & Fine-grained MT)

A cycle-accurate simulator for a simple multithreaded CPU that supports two micro-architectures:
**Blocked MT** (context switches on stalls, with a configurable penalty) and
**Fine-grained MT** (round-robin thread switch every cycle, no penalty). The ISA includes
`ADD/SUB/ADDI/SUBI`, `LOAD/STORE` (multi-cycle to memory), and `HALT`. :contentReference[oaicite:0]{index=0}

---

## 📘 Overview

- **Execution model.** One instruction can be issued per cycle (CPIₘₐₓ=1). Arithmetic ops take 1 cycle;
  memory ops incur an additional latency (from the config). There are **8 GPRs (R0–R7)** per thread. :contentReference[oaicite:1]{index=1}
- **Blocked MT.** On a stall, the core **may switch** to a ready thread; a **context-switch penalty** (O{x})
  is charged on each switch. :contentReference[oaicite:2]{index=2}
- **Fine-grained MT.** **Round-robin every cycle**, skipping halted/non-ready threads (flexible RR), **no penalty**. :contentReference[oaicite:3]{index=3}
- **Memory system.** Provided as a simulator: loads/stores are asynchronous w.r.t. other threads; image files
  (`.img`) define code/data segments and parameters (L{x}, S{x}, O{x}, N{x}). API in `sim_api.h/.c`. :contentReference[oaicite:4]{index=4} :contentReference[oaicite:5]{index=5}

---

## 🧩 Public API (what you implement)

The simulator exposes the following C API (see `core_api.h`): :contentReference[oaicite:6]{index=6}

```c
/* Run the simulation */
void   CORE_BlockedMT(void);
void   CORE_FinegrainedMT(void);

/* Copy out a thread's register-file after simulation */
void   CORE_BlockedMT_CTX(tcontext ctx[], int threadid);
void   CORE_FinegrainedMT_CTX(tcontext ctx[], int threadid);

/* Return overall performance (CPI) and free internal allocations */
double CORE_BlockedMT_CPI(void);
double CORE_FinegrainedMT_CPI(void);
```
The provided main.c uses this interface to load a memory image, run both modes,
print register files per thread, and report CPI.

---

## 🛠️ Build
Use the given makefile or compile manually:
```bash
# with makefile (recommended)
make          # produces: sim_main
make clean

# or manual build
cc -O2 -std=c11 sim_api.c core_api.c main.c -o sim_main
```
The memory/parameter APIs are declared in sim_api.h and implemented in sim_api.c.

---

## ▶️ Run

```bash
./sim_main <test_file.img>
# example:
./sim_main tests/example1.img
```
The test image defines: number of threads (N{x}), load/store latency (L{x}/S{x}),
and blocked-switch penalty (O{x}); plus per-thread code/data segments. The PDF includes an illustrated
timeline for **example1.img** and shows the expected CPI for both modes.

---

## 🧱 Implementation Notes

- **Per-thread state.** Each thread tracks {pc, delay, halted, regs[8]}. While a thread waits for a
LOAD/STORE, its delay counts down; other threads can run. (See core_api.c.) 
- **Blocked MT policy.** Try the current thread; if it is halted or waiting, search next ready thread in RR order.
- On a successful switch, add **O{x}** cycles and decrement all threads’ delays each cycle. 
- **Fine-grained policy.** Pick the next ready thread every cycle (skip halted/waiting). No switch penalty. 
- **Memory interface.** Instructions are fetched with SIM_MemInstRead(pc, &ins, tid). Data ops use
SIM_MemDataRead/Write(address, &val); effective addresses may use an immediate in src2. 
- **CPI reporting.** CPI is computed as cycles / committed_insts for each mode; the API call also frees
internal allocations. (See CORE_*_CPI implementation.)

---

## 📂 Project Structure
```bash
mt-simulator/
├─ src/
│  ├─ core_api.c      # full MT simulator (Blocked & Fine-grained) — your implementation
│  ├─ core_api.h      # simulator API (given)
│  ├─ sim_api.c       # memory + parameters simulator (given)
│  ├─ sim_api.h
│  └─ main.c          # CLI harness (loads .img, runs both modes, prints RF + CPI)
├─ tests/             # given by course staff
├─ tools/
│  └─ makefile
├─ .gitignore
├─ LICENSE
└─ README.md
```

---

## 🚫 Not Included
Course handout (CompArch-hw4.pdf) and official test materials are **not**
part of this repo. The simulator adheres to the interface and behavior defined in the spec.
