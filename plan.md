# Rust Port Plan: `ft_sched`

Porting the Fault-Tolerance scheduling thesis from C++20 to Rust, for IEEE publication.

## Guiding constraints

1. **Must run on the STM32F411** (real hardware, not just a host sim).
2. **Preemptive** scheduling is a thesis assumption — no async-only, no cooperative-only.
   (Preemptive subsumes cooperative, so a preemptive impl also covers the cooperative case.)
3. **Minimal dependencies, zero ST software.** ST *hardware* is fine; their HAL / CubeMX /
   USB device stack / FreeRTOS port are all out.
4. `base.hpp` is mostly **discarded, not ported** — lean on Rust's `core` / `std`.
5. Focus on **structure**: the seam that takes a closure / fn-pointer and wraps it in an FT
   strategy. Image code and test cases are explicitly out of scope for this plan.

## Why the rewrite (pain points being deleted)

- `USB_DEVICE` / USB-CDC serial → replaced by **RTT over the SWD link we already flash with**.
  USB is removed entirely.
- FreeRTOS + ST HAL + CubeMX → replaced by either a tiny custom preemptive microkernel or RTIC
  (see decision below). No ST-authored runtime code.
- `printf` plumbing → `core::fmt` / `defmt`.

---

## Dependency policy (what kills the "ST bullshit")

**Allowed (ARM / community, not ST):**
- `cortex-m` — SysTick, SCB, NVIC, MPU access.
- `cortex-m-rt` — vector table + startup / `.data`/`.bss` init.
- `critical-section` — critical regions via BASEPRI masking.
- `defmt` + `defmt-rtt` — logging over SWD (no USB).
- `heapless` — fixed-capacity collections (replaces `List`, `SPSC_Queue`).

**Borderline (your call):**
- `stm32f4` PAC (community `stm32-rs`, auto-generated from ST's SVD). Needed only for RCC (clock),
  FLASH (wait states), and one USART if we ever want UART instead of RTT. Recommendation: use the
  PAC *only* for clock/flash bring-up; everything else is pure `cortex-m`. If you want literally
  zero ST-derived crates, we hand-roll ~5 register pokes instead.

**Banned:** `stm32f4xx-hal`, CubeMX output, any USB stack, FreeRTOS.

**`no_std` on the device**, no global allocator unless explicitly opted in. Determinism story
(no hidden allocation) preserved via a small bump arena.

---

## Preemption strategy (KEY DECISION)

The thesis assumes **preemptive, thread-per-task scheduling** with two non-negotiable properties:
(a) tasks can **block / sleep / yield** mid-execution and resume, and (b) the supervisor can
**forcibly cancel** a runaway task. The Rust preemptive-RTOS landscape is thin (most results —
Embassy, RTIC — are cooperative/async or run-to-completion), but it is not empty.

### Options, in evaluation order

**1. R3 (`r3_kernel` + `r3_port_arm_m`) — TRYING FIRST.**
A pure-Rust RTOS with **preemptive priority scheduling + round-robin within a priority**, a
Cortex-M4F port, and blocking primitives (semaphores/mutexes/timers). R3 tasks are real threads with
their own stacks that block and preempt — so both thesis properties (a) and (b)-ish hold.
- **No HAL integration in the usual sense:** R3 *replaces* the HAL+RTOS scheduling/timing role. Its
  `use_port!` macro **owns SysTick and PendSV**. You provide: `cortex-m-rt`, the `stm32f4` PAC (or HAL
  only for boot-time clock setup), RCC PLL clock init (~5 register pokes), and the CPU/SysTick
  `FREQUENCY` const for the port. The F411 is a Cortex-M4F identical to the F401 R3 already has board
  examples for — near drop-in.
- **The real friction — static task declaration.** R3 tasks are declared in a compile-time const
  configuration, *not* spawned anonymously from a closure in an arena at runtime. For the fixed
  experiment shape (1 main + 3 TMR workers + 1 supervisor) a static task pool is fine. For an
  arbitrary `make_basic_task(|ctx| ...)` API it is a mismatch — milder than RTIC's, but present.
  **This is what the spike must evaluate.**
- **Other friction:** don't let a HAL `delay` also claim SysTick; match the `FREQUENCY` const to the
  real PLL clock exactly (mismatch = silently wrong deadline timing); don't `#[exception]`
  SysTick/PendSV yourself (`use_port!` claims them).
- **Spike scope:** boot + clock init + `use_port!` + one blinky task + a second task that preempts it.
  If that runs and the static-pool task model fits the FT API, R3 collapses Phase 5 to "configure a
  crate." If it fights the API too hard, fall back to option 2.

**2. Custom round-robin microkernel (PRIMARY FALLBACK / strong thesis fit).**
Thread/stack-per-task, FreeRTOS-like, but ours: ~150–250 lines, no priorities (round-robin
time-slicing only, which removes the hardest ~40%). Blocking, `sleep`, `yield`, and **forced cancel**
map **1:1** to the existing C++ design.
- Five pieces: SysTick handler (pends the switch), **PendSV context switch** (~18 instructions of
  `global_asm!` — the only asm, and the only genuinely risky part), the round-robin pick (plain Rust),
  stack init (fake exception frame), and bootstrap (point PSP at task 0, set CONTROL to PSP/thread).
- Tasks run on PSP, kernel/handlers on MSP. FPU `s16-s31` handled via the `tst lr,#0x10` lazy-stacking
  dance (can be deferred by avoiding float in tasks early on).
- **Why it's attractive for a thesis:** cancel = mark TCB dead + reclaim stack (a few lines); block =
  set state + pend PendSV. A ~200-line auditable scheduler is *describable in the paper* — a
  contribution and an appendix, not a black box. The MPU stack-guard angle (`NOTES`) bolts straight on.
- **Effort:** a long weekend to a week, time dominated by debugging the asm (one wrong register =
  HardFault with little context), 8-byte stack alignment, and the handler/Rust shared-state race.

**3. `freertos-rust` / freertos-next (SAFETY NET).**
Real, battle-tested preemptive scheduling. FreeRTOS is **not ST**, so it satisfies the hard "no ST"
rule — but it brings back the FreeRTOS config chores the rewrite is trying to escape. Use only if both
above stall.

### Recommendation / decision
**Spike R3 first** (option 1). If the static-task-pool model fits the FT API, use it and skip the
microkernel. Otherwise build the **custom round-robin kernel** (option 2) — it is the smallest thing
that satisfies *all* thesis assumptions with near-zero deps and the best paper story. **freertos-next**
is the net. **RTIC and Embassy are out**: RTIC v2 software tasks are `async` (rejected by constraint
#2) and RTIC v1 is run-to-completion (no in-task blocking, no forced cancel of a running task);
Embassy is cooperative/async. See footnote.

> **Footnote — why not RTIC/Embassy.** Both provide excellent *preemption of one priority by another*
> but not *thread-per-task blocking + forced cancellation of an in-progress task*. RTIC tasks are
> run-to-completion with no per-task stack to tear down; Embassy is cooperative async. Adopting either
> would require restructuring the thesis FT model toward message-passing/state-machines — a larger
> conceptual change than writing a context switch.

---

## The seam: one trait, two backends

Everything hinges on a single abstraction replacing `RawTask::_platform_*` and the free functions
(`tick_now`, `sleep_for`, `task_yield`):

```rust
pub trait Platform {
    fn now() -> Tick;
    fn tick_hz() -> u32;
    fn sleep(d: Duration);
    fn yield_now();
    fn critical<R>(f: impl FnOnce() -> R) -> R;   // replaces Spinlock / interrupt-free

    // raw task lifecycle — what FreeRTOS / pthread / Win32 used to provide
    fn spawn(desc: &mut RawTask) -> bool;
    fn join(task: &RawTask);
    fn cancel(task: &RawTask);                     // preemptive kill
}
```

`RawTask` carries the same data as today (entry fn pointer, arg pointer, stack region, status atomic,
optional `on_cancel`), minus the 16-byte opaque `_specific` blob — backend-specific state lives in the
backend's own TCB table, keyed by task id.

**Closures.** Like the C++ `BasicTask<Output, F, OnCancel>`, the typed wrapper owns the closure and a
result slot, is placed in an arena, and exposes a monomorphized `extern "C" fn(*mut ())` trampoline as
the raw entry. Public API stays: `make_basic_task(arena, stack, |ctx| { ... })`.

---

## Workspace layout

```
ft_sched/
  crates/
    ft-sched/         # no_std core: FT strategies, generic over Platform. THE THESIS.
    ft-sched-host/    # std backend: OS threads (dev + debugging on PC)
    ft-sched-cortexm/ # custom preemptive microkernel + STM32F411 wiring (or RTIC backend)
  examples/           # sobel etc. (later, out of current scope)
  memory.x            # linker layout for the F411
```

The core crate never names a platform; the two backends implement the one trait.

---

## Phased plan

### Phase 0 — Scaffold
- Cargo workspace, three crates, `memory.x` for the F411 (512K flash @ `0x0800_0000`,
  128K RAM @ `0x2000_0000`).
- Build targets: host (native) + `thumbv7em-none-eabihf` (Cortex-M4F hard-float, matches `fpv4-sp-d16`).
- Replace `build.lua` with `cargo`. CRC LUT becomes a compile-time `const fn` table — deletes
  `crc32_lut.gen.cpp` entirely.

### Phase 1 — Minimal core types (replace, don't port, `base.hpp`)
- **Keep**: `Duration` / `Tick` fixed-point design (clean, `no_std`-friendly) as a small newtype module.
- **Keep**: a minimal bump `Arena` (~60 lines); regions → a guard that restores `offset` on `Drop`.
- **Drop**: `Slice`→`&[T]`, `Option`→`Option`, `Array`→`[T;N]`, `String`/printf→`core::fmt`/`defmt`,
  `List`/`SPSC_Queue`→`heapless`, `Pair`→tuples, `defer`→`Drop`.
- Port `crc32.cpp` as a `const fn` table + `advance`/`oneshot`. Trait-ify integrity checking with a
  `Crc32` trait (mirrors `CRC32<T>` specializations).

### Phase 2 — Host backend first (fast feedback loop)
- Implement `Platform` for host using `std::thread` for spawn/join. Makes the **entire FT layer
  testable on PC** before touching the MCU.
- **Cancellation decision (host):** safe std can't force-kill a thread. To preserve
  `pthread_cancel` / `TerminateThread` semantics, drop to raw `libc::pthread_cancel` (Linux) /
  `TerminateThread` (Windows) via the thread handle — a faithful 1:1 mirror, isolated in `unsafe`
  in this crate. (Alternative: cooperative cancel points; weaker parity. Recommendation: raw mirror.)
- Port `tick_now` / `sleep_for` / `yield` from the Linux/Windows platform files.

### Phase 3 — FT core, generic over `Platform`
- `RawTask` + `make_raw_task` / `init_raw_task`.
- `BasicTask<Out, F>` + `TaskContext` (`ensure` / `panic` / `reset_deadline` / `id`).
- `DeadlineWatcher` / `DeadlineSlot` — fixed-slot supervisor with cancel callbacks and `scan()`.
  Backed by `heapless` + a `critical`-section lock instead of `Spinlock`.
- `software_watchdog` — supervisor task that traps on timeout (on device also kicks IWDG; Phase 7).
- Validate all of this on the **host** backend.

### Phase 4 — FT strategies (closure → strategy combinators)
The public surface: "take a closure and add whatever strategy."
- `make_basic_task` (baseline).
- **Re-execution**: run a closure N times sequentially + generic `consensus` / majority-vote helper.
  Pure, no threads.
- **TMR** (`TripleTask` / `make_tmr_task`): 3 subtasks under a sub-watcher, parallel, voted. Requires
  genuine preemptive tasks — the reason both backends must provide real preemption.
- Generic `with_crc(...)` wrapper adding integrity-check + selective recompute around any strategy.

At this point the thesis is fully reproduced and runs on PC. Everything below is bare-metal bring-up.

### Phase 5 — Device preemptive layer (the real work)

**Phase 5a — R3 spike (do this first).**
1. **Boot + clock**: `cortex-m-rt`, RCC PLL to 100 MHz (F411 max) + flash wait states via PAC
   (or HAL `.freeze()` only at boot). Can start on HSI-16 and optimize later.
2. **Port config**: `r3_port_arm_m::use_port!`, set the CPU/SysTick `FREQUENCY` const to match the
   real clock. Do **not** define SysTick/PendSV exceptions yourself; the port claims them.
3. **Smoke test**: one blinky task + a second task that preempts it; confirm round-robin time-slicing.
4. **Fit check**: express the FT shape (main + 3 TMR workers + supervisor) as R3's **static task pool**.
   Decide whether the static-declaration model is acceptable vs the dynamic closure-spawn API. If yes →
   implement `Platform` over R3 and skip 5b. If no → 5b.

**Phase 5b — custom round-robin microkernel (fallback if R3's task model doesn't fit).**
1. **Boot**: `cortex-m-rt` reset, `.data`/`.bss` init, vector table.
2. **Clock + flash**: as 5a.
3. **Time base**: SysTick at fixed tick rate → `now()` / `tick_hz()`; SysTick pends PendSV per quantum.
4. **Context switch** (heart): tasks in thread mode on **PSP**, kernel/handlers on **MSP**. **PendSV**
   at lowest priority saves `r4–r11` (+ `s16–s31` FPU, lazy stacking) to outgoing PSP, calls
   `switch_context()` to pick the next TCB, restores. ~18 instructions `global_asm!` + Rust picker.
5. **TCB table**: fixed array `{ psp, stack_region, state, id }`, no allocation; stacks from the arena.
6. **Scheduler**: round-robin only (no priorities). `yield_now()` and SysTick quantum pend PendSV.
7. **Blocking**: `sleep(d)` marks blocked-until-tick; scheduler skips it.
8. **Critical sections**: `critical-section`/BASEPRI → `Platform::critical`, watcher lock, watchdog.
9. **spawn/join/cancel**: spawn builds a fake exception frame so first PendSV "returns" into the entry;
   cancel removes TCB under a critical section and runs `on_cancel` (controlled analog of `vTaskDelete`).
   The deadline supervisor runs as a high-priority task (or in SysTick) so it can preempt a runaway task.

**Last resort — freertos-next:** if both 5a and 5b stall, bind C FreeRTOS (not ST). Reintroduces
FreeRTOS config chores; chosen only under schedule pressure.

### Phase 6 — STM32 backend wiring
- Implement `Platform` for the device by delegating to the Phase-5 layer.
- **Logging** via `defmt` + `defmt-rtt` over SWD — replaces every `printf` / USB-CDC path.
  `dump_bitmap` / serial results become RTT. **`USB_DEVICE` is gone.**
- `trap()` → `SCB::sys_reset()` (matches `HAL_NVIC_SystemReset`), no HAL.

### Phase 7 — Hardware-assisted fault tolerance (optional; `NOTES` demands the MPU)
`NOTES`: *"USAR A OPCAO COM MPU (INCLUI ISSO NO TUTORIAL!!!!!!!)"*. Strong paper angle, pure `cortex-m`,
zero ST:
- **MPU**: per-task stack guard regions → overflow raises MemManage → recover/cancel faulting task.
  Hardware fault isolation as an FT mechanism.
- **IWDG**: independent hardware watchdog (register-level), kicked by the software-watchdog task.
  Defense in depth if the whole kernel hangs.

### Phase 8 — Examples / experiments (out of current scope, noted)
Sobel workload, the experiment matrix from `NOTES` (baseline / crc / tmr / reexec × pure/monitored),
and the pyOCD fault-injection harness (`ft_inject.py`) carry over essentially unchanged — pyOCD reads
ELF symbols, so it works against a Rust binary. APIs in Phases 3–4 are designed so these drop in later.

---

## Risks & open decisions

- **Biggest risk: Phase 5 context switch.** Mitigation: finish Phases 1–4 + host backend first so the
  thesis logic is proven and reproducible on PC; attack the microkernel as a self-contained milestone.
  Even if the kernel slips, there is a working reference implementation for the paper.
- **Open decision 1 — preemption layer:** spike **R3** first; if its static-task-pool model fits the
  FT API, use it, else build the **custom round-robin kernel**. freertos-next is the net. RTIC/Embassy
  out. Settle in Phase 5a.
- **Open decision 2 — host cancellation:** raw OS-thread kill (faithful to C++) vs cooperative cancel
  points. Recommendation: faithful raw kill.
- **Open decision 3 — `stm32f4` PAC:** acceptable (community, register-only, for clock bring-up) vs
  literally zero ST-derived crates (hand-rolled register pokes).

## Out of scope (per current instructions)
- Image processing (`image.cpp`, Sobel), Bitmap/PGM.
- Test cases / experiment harness.
- `wav.cpp`, `injection/ft_inject.py`, `tools/png2p5.py` (unused / deferred).
