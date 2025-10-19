/* 046267 Computer Architecture - HW #4 */

#include "core_api.h"
#include "sim_api.h"
#include <stdlib.h>
#include <stdbool.h>

// Per-thread state
typedef struct {
    int delay;         // remaining wait cycles
    bool halted;       // has the thread executed HALT?
    int pc;            // program counter (instruction index)
    tcontext ctx;      // register context
} Task;

// Globals for blocked MT
static Task       *blocked_tasks    = NULL;
static long        blocked_cycles   = 0;
static long        blocked_insts    = 0;

// Globals for fine-grained MT
static Task       *fine_tasks       = NULL;
static long        fine_cycles      = 0;
static long        fine_insts       = 0;

// Helper: decrement delay for all tasks
static void decrement_all(Task *arr, int n) {
    for (int i = 0; i < n; i++) {
        if (arr[i].delay > 0)
            arr[i].delay--;
    }
}

void CORE_BlockedMT(void) {
    int n = SIM_GetThreadsNum();
    int ld = SIM_GetLoadLat() + 1;
    int st = SIM_GetStoreLat() + 1;
    int sw = SIM_GetSwitchCycles();

    // allocate and init
    blocked_tasks = calloc(n, sizeof(Task));
    for (int i = 0; i < n; i++) {
        blocked_tasks[i].delay  = 0;
        blocked_tasks[i].halted = false;
        blocked_tasks[i].pc     = 0;
        for (int r = 0; r < REGS_COUNT; r++)
            blocked_tasks[i].ctx.reg[r] = 0;
    }

    blocked_cycles = 0;
    blocked_insts  = 0;
    int cur = 0;
    bool idle = false;

    while (true) {
        // check if done
        bool all_done = true;
        for (int i = 0; i < n; i++)
            if (!blocked_tasks[i].halted) { all_done = false; break; }
        if (all_done) break;

        if (idle) {
            blocked_cycles++;
            decrement_all(blocked_tasks, n);
            idle = false;
            continue;
        }

        Task *t = &blocked_tasks[cur];
        if (t->halted || t->delay > 0) {
            // try to switch
            bool found = false;
            for (int d = 1; d < n; d++) {
                int nxt = (cur + d) % n;
                if (!blocked_tasks[nxt].halted && blocked_tasks[nxt].delay == 0) {
                    cur = nxt;
                    for (int k = 0; k < sw; k++) {
                        blocked_cycles++;
                        decrement_all(blocked_tasks, n);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                idle = true;
            }
            continue;
        }

        // fetch & exec
        Instruction ins;
        SIM_MemInstRead(t->pc, &ins, cur);
        blocked_cycles++;
        blocked_insts++;

        int dst = ins.dst_index;
        int s1  = ins.src1_index;
        int s2  = ins.src2_index_imm;
        bool isImm = ins.isSrc2Imm;

        switch (ins.opcode) {
            case CMD_NOP:
                t->pc++;
                break;

            case CMD_ADD:
                t->ctx.reg[dst] = t->ctx.reg[s1] + t->ctx.reg[s2];
                t->pc++;
                break;

            case CMD_SUB:
                t->ctx.reg[dst] = t->ctx.reg[s1] - t->ctx.reg[s2];
                t->pc++;
                break;

            case CMD_ADDI:
                t->ctx.reg[dst] = t->ctx.reg[s1] + s2;
                t->pc++;
                break;

            case CMD_SUBI:
                t->ctx.reg[dst] = t->ctx.reg[s1] - s2;
                t->pc++;
                break;

            case CMD_LOAD:
                if (isImm)
                    SIM_MemDataRead(t->ctx.reg[s1] + s2, &t->ctx.reg[dst]);
                else
                    SIM_MemDataRead(t->ctx.reg[s1] + t->ctx.reg[s2], &t->ctx.reg[dst]);
                t->delay = ld;
                t->pc++;
                break;

            case CMD_STORE:
                if (isImm)
                    SIM_MemDataWrite(t->ctx.reg[dst] + s2, t->ctx.reg[s1]);
                else
                    SIM_MemDataWrite(t->ctx.reg[dst] + t->ctx.reg[s2], t->ctx.reg[s1]);
                t->delay = st;
                t->pc++;
                break;

            case CMD_HALT:
                t->halted = true;
                break;

            default:
                t->pc++;
                break;
        }

        decrement_all(blocked_tasks, n);
    }
}

void CORE_FinegrainedMT(void) {
    int n = SIM_GetThreadsNum();
    int ld = SIM_GetLoadLat() + 1;
    int st = SIM_GetStoreLat() + 1;

    fine_tasks = calloc(n, sizeof(Task));
    for (int i = 0; i < n; i++) {
        fine_tasks[i].delay  = 0;
        fine_tasks[i].halted = false;
        fine_tasks[i].pc     = 0;
        for (int r = 0; r < REGS_COUNT; r++)
            fine_tasks[i].ctx.reg[r] = 0;
    }

    fine_cycles = 0;
    fine_insts  = 0;
    int cur = 0;

    while (true) {
        // termination?
        bool all_done = true;
        for (int i = 0; i < n; i++)
            if (!fine_tasks[i].halted) { all_done = false; break; }
        if (all_done) break;

        // pick next ready
        bool ready = false;
        for (int d = 0; d < n; d++) {
            int cand = (cur + d) % n;
            if (!fine_tasks[cand].halted && fine_tasks[cand].delay == 0) {
                cur = cand;
                ready = true;
                break;
            }
        }
        if (!ready) {
            fine_cycles++;
            decrement_all(fine_tasks, n);
            continue;
        }

        // fetch & exec
        Instruction ins;
        SIM_MemInstRead(fine_tasks[cur].pc, &ins, cur);
        fine_cycles++;
        fine_insts++;

        int dst = ins.dst_index;
        int s1  = ins.src1_index;
        int s2  = ins.src2_index_imm;
        bool isImm = ins.isSrc2Imm;

        switch (ins.opcode) {
            case CMD_NOP:
                break;

            case CMD_ADD:
                fine_tasks[cur].ctx.reg[dst] = fine_tasks[cur].ctx.reg[s1]
                                            + fine_tasks[cur].ctx.reg[s2];
                break;

            case CMD_SUB:
                fine_tasks[cur].ctx.reg[dst] = fine_tasks[cur].ctx.reg[s1]
                                            - fine_tasks[cur].ctx.reg[s2];
                break;

            case CMD_ADDI:
                fine_tasks[cur].ctx.reg[dst] = fine_tasks[cur].ctx.reg[s1] + s2;
                break;

            case CMD_SUBI:
                fine_tasks[cur].ctx.reg[dst] = fine_tasks[cur].ctx.reg[s1] - s2;
                break;

            case CMD_LOAD:
                if (isImm)
                    SIM_MemDataRead(fine_tasks[cur].ctx.reg[s1] + s2,
                                    &fine_tasks[cur].ctx.reg[dst]);
                else
                    SIM_MemDataRead(fine_tasks[cur].ctx.reg[s1]
                                    + fine_tasks[cur].ctx.reg[s2],
                                    &fine_tasks[cur].ctx.reg[dst]);
                fine_tasks[cur].delay = ld;
                break;

            case CMD_STORE:
                if (isImm)
                    SIM_MemDataWrite(fine_tasks[cur].ctx.reg[dst] + s2,
                                     fine_tasks[cur].ctx.reg[s1]);
                else
                    SIM_MemDataWrite(fine_tasks[cur].ctx.reg[dst]
                                     + fine_tasks[cur].ctx.reg[s2],
                                     fine_tasks[cur].ctx.reg[s1]);
                fine_tasks[cur].delay = st;
                break;

            case CMD_HALT:
                fine_tasks[cur].halted = true;
                break;

            default:
                break;
        }

        fine_tasks[cur].pc++;
        decrement_all(fine_tasks, n);
        cur = (cur + 1) % n;
    }
}

double CORE_BlockedMT_CPI(void) {
    free(blocked_tasks);
    return (blocked_insts == 0 ? 0.0
           : (double)blocked_cycles / blocked_insts);
}

double CORE_FinegrainedMT_CPI(void) {
    free(fine_tasks);
    return (fine_insts == 0 ? 0.0
           : (double)fine_cycles / fine_insts);
}

void CORE_BlockedMT_CTX(tcontext ctx[], int tid) {
    for (int r = 0; r < REGS_COUNT; r++)
        ctx[tid].reg[r] = blocked_tasks[tid].ctx.reg[r];
}

void CORE_FinegrainedMT_CTX(tcontext ctx[], int tid) {
    for (int r = 0; r < REGS_COUNT; r++)
        ctx[tid].reg[r] = fine_tasks[tid].ctx.reg[r];
}