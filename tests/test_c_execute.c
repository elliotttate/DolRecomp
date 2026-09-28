#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../src/cpu/cpu.h"

void func_80004020(CPUState* ctx);
void func_80004040(CPUState* ctx);
void func_80004060(CPUState* ctx);
void func_80004068(CPUState* ctx);
void func_80004070(CPUState* ctx);
void func_800040A0(CPUState* ctx);
void func_800040C0(CPUState* ctx);
void func_800040E0(CPUState* ctx);
void func_80004140(CPUState* ctx);
int dolrecomp_test_dispatch(CPUState* ctx);

#define TEST_PPC_MSR_EE 0x00008000u

typedef struct QuantumResult {
    u32 pc;
    u32 r3;
    u32 r4;
    u32 cr;
    u64 elapsed;
} QuantumResult;

static QuantumResult run_quantum_loop(s64 cap, u64 deadline) {
    CPUState cpu;
    QuantumResult result = {0};
    if (!cpu_init(&cpu))
        return result;

    cpu.pc = 0x800040C0u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 1000u;
    cpu.gpr[4] = 3u;
    cpu.gpr[5] = 7u;
    while (result.elapsed < deadline && cpu.pc != cpu.lr) {
        const u64 distance = deadline - result.elapsed;
        cpu.downcount = 0;
        cpu.cycle_budget = distance < (u64)cap ? (s64)distance : cap;
        cpu.cycle_deadline_budget = (s64)distance;
        func_800040C0(&cpu);
        if (cpu.downcount >= 0)
            break;
        result.elapsed += (u64)(-cpu.downcount);
    }
    result.pc = cpu.pc;
    result.r3 = cpu.gpr[3];
    result.r4 = cpu.gpr[4];
    result.cr = cpu.cr;
    cpu_free(&cpu);
    return result;
}

typedef struct ObservationRun {
    u64 absolute;
    u64 deadline;
    s64 cap;
} ObservationRun;

static ObservationRun* active_observation;

static u64 observe_external_read(CPUState* cpu, u32 ea, u8 size) {
    (void)ea;
    (void)size;
    const u64 charged = cpu->downcount < 0 ? (u64)(-cpu->downcount) : 0u;
    const u64 suffix = cpu->cycle_observation_suffix < charged
                           ? cpu->cycle_observation_suffix
                           : charged;
    active_observation->absolute += charged - suffix;
    cpu->downcount = -(s64)suffix;
    const u64 distance = active_observation->deadline > active_observation->absolute
                             ? active_observation->deadline - active_observation->absolute
                             : 1u;
    cpu->cycle_budget = distance < (u64)active_observation->cap
                            ? (s64)distance
                            : active_observation->cap;
    cpu->cycle_deadline_budget = (s64)distance;
    return 7u;
}

static QuantumResult run_observed_loop(s64 cap, u64 deadline) {
    CPUState cpu;
    QuantumResult result = {0};
    ObservationRun observation = {.deadline = deadline, .cap = cap};
    if (!cpu_init(&cpu))
        return result;

    active_observation = &observation;
    cpu.external_read = observe_external_read;
    cpu.pc = 0x800040E0u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 1000u;
    cpu.gpr[4] = 3u;
    cpu.gpr[5] = 0xCC005004u;
    while (observation.absolute < deadline && cpu.pc != cpu.lr) {
        const u64 distance = deadline - observation.absolute;
        cpu.downcount = 0;
        cpu.cycle_budget = distance < (u64)cap ? (s64)distance : cap;
        cpu.cycle_deadline_budget = (s64)distance;
        func_800040E0(&cpu);
        if (cpu.downcount < 0)
            observation.absolute += (u64)(-cpu.downcount);
        else
            break;
    }
    result.pc = cpu.pc;
    result.r3 = cpu.gpr[3];
    result.r4 = cpu.gpr[4];
    result.cr = cpu.cr;
    result.elapsed = observation.absolute;
    active_observation = NULL;
    cpu_free(&cpu);
    return result;
}

static QuantumResult run_cross_dispatch(s64 cap, u64 deadline) {
    CPUState cpu;
    QuantumResult result = {0};
    if (!cpu_init(&cpu))
        return result;

    cpu.pc = 0x80004100u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 1000u;
    cpu.gpr[4] = 3u;
    cpu.gpr[5] = 7u;
    while (result.elapsed < deadline) {
        const u64 distance = deadline - result.elapsed;
        cpu.downcount = 0;
        cpu.cycle_budget = distance < (u64)cap ? (s64)distance : cap;
        cpu.cycle_deadline_budget = (s64)distance;
        if (!dolrecomp_test_dispatch(&cpu))
            break;
        if (cpu.downcount >= 0)
            break;
        result.elapsed += (u64)(-cpu.downcount);
    }
    result.pc = cpu.pc;
    result.r3 = cpu.gpr[3];
    result.r4 = cpu.gpr[4];
    result.cr = cpu.cr;
    cpu_free(&cpu);
    return result;
}

static QuantumResult run_indirect_dispatch(s64 cap, u64 deadline) {
    CPUState cpu;
    QuantumResult result = {0};
    if (!cpu_init(&cpu))
        return result;

    cpu.pc = 0x80004120u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 1000u;
    cpu.gpr[5] = 7u;
    cpu.gpr[11] = cpu.lr;
    cpu.gpr[12] = 0x80005120u;
    while (result.elapsed < deadline && cpu.pc != cpu.lr) {
        const u64 distance = deadline - result.elapsed;
        cpu.downcount = 0;
        cpu.cycle_budget = distance < (u64)cap ? (s64)distance : cap;
        cpu.cycle_deadline_budget = (s64)distance;
        if (!dolrecomp_test_dispatch(&cpu))
            break;
        if (cpu.downcount >= 0)
            break;
        result.elapsed += (u64)(-cpu.downcount);
    }
    result.pc = cpu.pc;
    result.r3 = cpu.gpr[3];
    result.r4 = cpu.gpr[4];
    result.cr = cpu.cr;
    cpu_free(&cpu);
    return result;
}

static QuantumResult run_interrupt_enable_boundary(s64 cap) {
    CPUState cpu;
    QuantumResult result = {0};
    if (!cpu_init(&cpu))
        return result;

    cpu.pc = 0x80004140u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 0u;
    cpu.gpr[4] = TEST_PPC_MSR_EE;
    cpu.cycle_budget = cap;
    cpu.downcount = 0;
    func_80004140(&cpu);
    result.pc = cpu.pc;
    result.r3 = cpu.gpr[3];
    result.r4 = cpu.msr;
    result.elapsed = cpu.downcount < 0 ? (u64)(-cpu.downcount) : 0u;
    cpu_free(&cpu);
    return result;
}

static u64 bits_of(f64 value) {
    u64 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int main(void) {
    CPUState cpu;
    if (!cpu_init(&cpu))
        return 1;

    cpu.pc = 0x80004020u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 1000;

    u32 calls = 0;
    while (cpu.pc != cpu.lr && calls < 32) {
        cpu.downcount = 0;
        func_80004020(&cpu);
        calls++;
    }

    int integer_ok = cpu.pc == cpu.lr && cpu.gpr[3] == 0 &&
                     (cpu.cr & 0xF0000000u) == 0x20000000u && calls < 20;
    if (!integer_ok) {
        fprintf(stderr, "pc=%08X r3=%u cr=%08X calls=%u\n",
                cpu.pc, cpu.gpr[3], cpu.cr, calls);
    }

    cpu.pc = 0x80004020u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 3u;
    cpu.downcount = 0;
    cpu.cycle_budget = 2;
    cpu.cycle_deadline_active = 1u;
    cpu.cycle_deadline_budget = 2;
    func_80004020(&cpu);
    int deadline_ok = cpu.pc == 0x80004028u && cpu.gpr[3] == 2u &&
                      cpu.downcount == -2;
    calls = 1u;
    while (cpu.pc != cpu.lr && calls < 32u) {
        cpu.downcount = 0;
        func_80004020(&cpu);
        calls++;
    }
    deadline_ok = deadline_ok && cpu.pc == cpu.lr && cpu.gpr[3] == 0u &&
                  (cpu.cr & 0xF0000000u) == 0x20000000u;
    if (!deadline_ok) {
        fprintf(stderr,
                "deadline pc=%08X r3=%u cr=%08X downcount=%lld calls=%u\n",
                cpu.pc, cpu.gpr[3], cpu.cr, (long long)cpu.downcount, calls);
    }
    cpu.cycle_deadline_active = 0u;
    cpu.cycle_deadline_budget = 0;
    cpu.cycle_budget = 0;

    cpu.pc = 0x800040A0u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 0u;
    cpu.cr = 0u;
    cpu.downcount = 0;
    cpu.cycle_budget = 2;
    cpu.cycle_deadline_budget = 3;
    func_800040A0(&cpu);
    int cap_plus_one_ok = cpu.pc == 0x800040A8u && cpu.gpr[3] == 2u &&
                          cpu.downcount == -2;
    cpu.downcount = 0;
    cpu.cycle_budget = 1;
    cpu.cycle_deadline_budget = 1;
    func_800040A0(&cpu);
    const u32 narrow_pc = cpu.pc;
    const u32 narrow_gpr3 = cpu.gpr[3];
    const u32 narrow_cr = cpu.cr;

    cpu.pc = 0x800040A0u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 0u;
    cpu.cr = 0u;
    cpu.downcount = 0;
    cpu.cycle_budget = 3;
    cpu.cycle_deadline_budget = 3;
    func_800040A0(&cpu);
    cap_plus_one_ok = cap_plus_one_ok && cpu.pc == narrow_pc &&
                      cpu.gpr[3] == narrow_gpr3 && cpu.cr == narrow_cr &&
                      cpu.pc == 0x800040ACu && cpu.gpr[3] == 3u;
    if (!cap_plus_one_ok) {
        fprintf(stderr,
                "cap+1 narrow=%08X/%u/%08X wide=%08X/%u/%08X\n",
                narrow_pc, narrow_gpr3, narrow_cr, cpu.pc, cpu.gpr[3],
                cpu.cr);
    }
    cpu.downcount = 0;
    cpu.cycle_budget = 0;
    cpu.cycle_deadline_budget = 0;

    const QuantumResult quantum_17 = run_quantum_loop(17, 263u);
    const QuantumResult quantum_64 = run_quantum_loop(64, 263u);
    const QuantumResult quantum_256 = run_quantum_loop(256, 263u);
    const int quantum_ok =
        quantum_17.pc == quantum_64.pc && quantum_17.pc == quantum_256.pc &&
        quantum_17.r3 == quantum_64.r3 && quantum_17.r3 == quantum_256.r3 &&
        quantum_17.r4 == quantum_64.r4 && quantum_17.r4 == quantum_256.r4 &&
        quantum_17.cr == quantum_64.cr && quantum_17.cr == quantum_256.cr &&
        quantum_17.elapsed == quantum_64.elapsed &&
        quantum_17.elapsed == quantum_256.elapsed;
    if (!quantum_ok) {
        fprintf(stderr,
                "quantum cap17=%08X/%u/%u/%08X/%llu "
                "cap64=%08X/%u/%u/%08X/%llu "
                "cap256=%08X/%u/%u/%08X/%llu\n",
                quantum_17.pc, quantum_17.r3, quantum_17.r4, quantum_17.cr,
                (unsigned long long)quantum_17.elapsed,
                quantum_64.pc, quantum_64.r3, quantum_64.r4, quantum_64.cr,
                (unsigned long long)quantum_64.elapsed,
                quantum_256.pc, quantum_256.r3, quantum_256.r4,
                quantum_256.cr, (unsigned long long)quantum_256.elapsed);
    }

    const QuantumResult observed_17 = run_observed_loop(17, 263u);
    const QuantumResult observed_64 = run_observed_loop(64, 263u);
    const QuantumResult observed_256 = run_observed_loop(256, 263u);
    const int observed_ok =
        observed_17.pc == observed_64.pc && observed_17.pc == observed_256.pc &&
        observed_17.r3 == observed_64.r3 && observed_17.r3 == observed_256.r3 &&
        observed_17.r4 == observed_64.r4 && observed_17.r4 == observed_256.r4 &&
        observed_17.cr == observed_64.cr && observed_17.cr == observed_256.cr &&
        observed_17.elapsed == observed_64.elapsed &&
        observed_17.elapsed == observed_256.elapsed;
    if (!observed_ok) {
        fprintf(stderr,
                "observed cap17=%08X/%u/%u/%08X/%llu "
                "cap64=%08X/%u/%u/%08X/%llu "
                "cap256=%08X/%u/%u/%08X/%llu\n",
                observed_17.pc, observed_17.r3, observed_17.r4, observed_17.cr,
                (unsigned long long)observed_17.elapsed,
                observed_64.pc, observed_64.r3, observed_64.r4, observed_64.cr,
                (unsigned long long)observed_64.elapsed,
                observed_256.pc, observed_256.r3, observed_256.r4,
                observed_256.cr, (unsigned long long)observed_256.elapsed);
    }

    const QuantumResult cross_3 = run_cross_dispatch(3, 263u);
    const QuantumResult cross_17 = run_cross_dispatch(17, 263u);
    const QuantumResult cross_256 = run_cross_dispatch(256, 263u);
    const int cross_ok =
        cross_3.pc == cross_17.pc && cross_3.pc == cross_256.pc &&
        cross_3.r3 == cross_17.r3 && cross_3.r3 == cross_256.r3 &&
        cross_3.r4 == cross_17.r4 && cross_3.r4 == cross_256.r4 &&
        cross_3.cr == cross_17.cr && cross_3.cr == cross_256.cr &&
        cross_3.elapsed == cross_17.elapsed &&
        cross_3.elapsed == cross_256.elapsed;
    if (!cross_ok) {
        fprintf(stderr,
                "cross cap3=%08X/%u/%u/%08X/%llu "
                "cap17=%08X/%u/%u/%08X/%llu "
                "cap256=%08X/%u/%u/%08X/%llu\n",
                cross_3.pc, cross_3.r3, cross_3.r4, cross_3.cr,
                (unsigned long long)cross_3.elapsed,
                cross_17.pc, cross_17.r3, cross_17.r4, cross_17.cr,
                (unsigned long long)cross_17.elapsed,
                cross_256.pc, cross_256.r3, cross_256.r4, cross_256.cr,
                (unsigned long long)cross_256.elapsed);
    }

    int indirect_ok = 1;
    for (u64 deadline = 1u; deadline <= 16u; ++deadline) {
        const QuantumResult indirect_3 = run_indirect_dispatch(3, deadline);
        const QuantumResult indirect_17 = run_indirect_dispatch(17, deadline);
        const QuantumResult indirect_256 = run_indirect_dispatch(256, deadline);
        const int deadline_ok =
            indirect_3.pc == indirect_17.pc &&
            indirect_3.pc == indirect_256.pc &&
            indirect_3.r3 == indirect_17.r3 &&
            indirect_3.r3 == indirect_256.r3 &&
            indirect_3.r4 == indirect_17.r4 &&
            indirect_3.r4 == indirect_256.r4 &&
            indirect_3.cr == indirect_17.cr &&
            indirect_3.cr == indirect_256.cr &&
            indirect_3.elapsed == indirect_17.elapsed &&
            indirect_3.elapsed == indirect_256.elapsed;
        if (!deadline_ok) {
            fprintf(stderr,
                    "indirect deadline=%llu cap3=%08X/%u/%u/%08X/%llu "
                    "cap17=%08X/%u/%u/%08X/%llu "
                    "cap256=%08X/%u/%u/%08X/%llu\n",
                    (unsigned long long)deadline, indirect_3.pc,
                    indirect_3.r3, indirect_3.r4, indirect_3.cr,
                    (unsigned long long)indirect_3.elapsed,
                    indirect_17.pc, indirect_17.r3, indirect_17.r4,
                    indirect_17.cr, (unsigned long long)indirect_17.elapsed,
                    indirect_256.pc, indirect_256.r3, indirect_256.r4,
                    indirect_256.cr, (unsigned long long)indirect_256.elapsed);
            indirect_ok = 0;
            break;
        }
    }

    const QuantumResult enable_2 = run_interrupt_enable_boundary(2);
    const QuantumResult enable_256 = run_interrupt_enable_boundary(256);
    const int interrupt_enable_ok =
        enable_2.pc == 0x80004148u && enable_256.pc == enable_2.pc &&
        enable_2.r3 == 1u && enable_256.r3 == enable_2.r3 &&
        enable_2.r4 == TEST_PPC_MSR_EE && enable_256.r4 == enable_2.r4 &&
        enable_2.elapsed == 2u && enable_256.elapsed == enable_2.elapsed;
    if (!interrupt_enable_ok) {
        fprintf(stderr,
                "interrupt-enable cap2=%08X/%u/%08X/%llu "
                "cap256=%08X/%u/%08X/%llu\n",
                enable_2.pc, enable_2.r3, enable_2.r4,
                (unsigned long long)enable_2.elapsed,
                enable_256.pc, enable_256.r3, enable_256.r4,
                (unsigned long long)enable_256.elapsed);
    }

    for (u32 i = 0; i < 1000; ++i)
        mem_write32(&cpu, 0x80001000u + i * 4u, i);
    cpu.pc = 0x80004040u;
    cpu.lr = 0x81234564u;
    cpu.gpr[3] = 1000;
    cpu.gpr[5] = 0x80001000u;
    calls = 0;
    while (cpu.pc != cpu.lr && calls < 32) {
        cpu.downcount = 0;
        func_80004040(&cpu);
        calls++;
    }
    int memory_ok = cpu.pc == cpu.lr && cpu.gpr[3] == 0 &&
                    cpu.gpr[4] == 999 && cpu.gpr[5] == 0x80001FA0u &&
                    calls < 24;
    if (!memory_ok) {
        fprintf(stderr, "memory pc=%08X r3=%u r4=%u r5=%08X calls=%u\n",
                cpu.pc, cpu.gpr[3], cpu.gpr[4], cpu.gpr[5], calls);
    }

    cpu.msr = 0x00002000u;
    cpu.lr = 0x81234564u;
    cpu.fpscr = 0;
    cpu.fpr[1] = NAN;
    cpu.fpr[2] = 1.0;
    cpu.pc = 0x80004060u;
    func_80004060(&cpu);
    int compare_ok = (cpu.fpscr & 0x00080000u) != 0 &&
                     ((cpu.fpscr >> 12) & 0xFu) == 1u &&
                     ((cpu.cr >> 20) & 0xFu) == 1u;

    cpu.fpscr = 0xA0000000u;
    cpu.cr = 0;
    cpu.fpr[1] = 1.25;
    cpu.fpr[2] = 2.5;
    cpu.pc = 0x80004068u;
    func_80004068(&cpu);
    int record_ok = cpu.fpr[3] == 3.75 && cpu.ps1[3] == 3.75 &&
                    ((cpu.cr >> 24) & 0xFu) == 0xAu;

    cpu.fpr[1] = 0x1.0000000000001p+0;
    cpu.fpr[2] = -0x1.0000000000001p+0;
    u64 merge_a = bits_of(cpu.fpr[1]);
    u64 merge_b = bits_of(cpu.fpr[2]);
    cpu.pc = 0x80004070u;
    func_80004070(&cpu);
    int merge_ok = bits_of(cpu.fpr[5]) == merge_a &&
                   bits_of(cpu.ps1[5]) == merge_b;

    if (!compare_ok || !record_ok || !merge_ok) {
        fprintf(stderr, "float compare=%d record=%d merge=%d fpscr=%08X cr=%08X\n",
                compare_ok, record_ok, merge_ok, cpu.fpscr, cpu.cr);
    }

    cpu_free(&cpu);
    return !(integer_ok && deadline_ok && cap_plus_one_ok && quantum_ok &&
             observed_ok && cross_ok && indirect_ok && interrupt_enable_ok &&
             memory_ok && compare_ok && record_ok && merge_ok);
}
