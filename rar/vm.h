/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2026 Natalia Portillo.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AARU_COMPRESSION_NATIVE_RAR_VM_H
#define AARU_COMPRESSION_NATIVE_RAR_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bitstream.h"

/*
 * RAR 3.0 virtual machine for post-decompression filters.
 *
 * 256KB address space, 8 general-purpose 32-bit registers, flags register.
 * 40 instructions with x86-like semantics.
 * Computed-goto threaded dispatch for performance.
 */

#define RAR_VM_MEM_SIZE      0x40000
#define RAR_VM_MEM_MASK      (RAR_VM_MEM_SIZE - 1)
#define RAR_VM_WORK_SIZE     0x3c000
#define RAR_VM_GLOBAL_SIZE   0x2000
#define RAR_VM_SYS_GLOBAL    RAR_VM_WORK_SIZE
#define RAR_VM_SYS_GLOBAL_SZ 64
#define RAR_VM_USR_GLOBAL    (RAR_VM_SYS_GLOBAL + RAR_VM_SYS_GLOBAL_SZ)
#define RAR_VM_USR_GLOBAL_SZ (RAR_VM_GLOBAL_SIZE - RAR_VM_SYS_GLOBAL_SZ)

/* Maximum instruction count to prevent infinite loops (DoS protection) */
#define RAR_VM_MAX_INSTRUCTIONS 250000000

/* Instruction opcodes */
enum
{
    RAR_VM_MOV              = 0,
    RAR_VM_CMP              = 1,
    RAR_VM_ADD              = 2,
    RAR_VM_SUB              = 3,
    RAR_VM_JZ               = 4,
    RAR_VM_JNZ              = 5,
    RAR_VM_INC              = 6,
    RAR_VM_DEC              = 7,
    RAR_VM_JMP              = 8,
    RAR_VM_XOR              = 9,
    RAR_VM_AND              = 10,
    RAR_VM_OR               = 11,
    RAR_VM_TEST             = 12,
    RAR_VM_JS               = 13,
    RAR_VM_JNS              = 14,
    RAR_VM_JB               = 15,
    RAR_VM_JBE              = 16,
    RAR_VM_JA               = 17,
    RAR_VM_JAE              = 18,
    RAR_VM_PUSH             = 19,
    RAR_VM_POP              = 20,
    RAR_VM_CALL             = 21,
    RAR_VM_RET              = 22,
    RAR_VM_NOT              = 23,
    RAR_VM_SHL              = 24,
    RAR_VM_SHR              = 25,
    RAR_VM_SAR              = 26,
    RAR_VM_NEG              = 27,
    RAR_VM_PUSHA            = 28,
    RAR_VM_POPA             = 29,
    RAR_VM_PUSHF            = 30,
    RAR_VM_POPF             = 31,
    RAR_VM_MOVZX            = 32,
    RAR_VM_MOVSX            = 33,
    RAR_VM_XCHG             = 34,
    RAR_VM_MUL              = 35,
    RAR_VM_DIV              = 36,
    RAR_VM_ADC              = 37,
    RAR_VM_SBB              = 38,
    RAR_VM_PRINT            = 39,
    RAR_VM_NUM_INSTRUCTIONS = 40
};

/* Addressing modes */
enum
{
    RAR_VM_REG0      = 0,
    RAR_VM_REG7      = 7,
    RAR_VM_REG_IND0  = 8,
    RAR_VM_REG_IND7  = 15,
    RAR_VM_IDX_ABS0  = 16,
    RAR_VM_IDX_ABS7  = 23,
    RAR_VM_ABSOLUTE  = 24,
    RAR_VM_IMMEDIATE = 25,
    RAR_VM_NUM_MODES = 26
};

/* VM state */
typedef struct
{
    uint32_t registers[8];
    uint32_t flags;
    uint8_t  memory[RAR_VM_MEM_SIZE + 3]; /* +3 for overflow read safety */
} rar_vm_t;

/* Single opcode */
typedef struct rar_vm_opcode_t rar_vm_opcode_t;

typedef uint32_t (*rar_vm_getter_t)(rar_vm_t *vm, uint32_t value);
typedef void (*rar_vm_setter_t)(rar_vm_t *vm, uint32_t value, uint32_t data);

struct rar_vm_opcode_t
{
    void *label;

    rar_vm_getter_t getter1;
    rar_vm_setter_t setter1;
    uint32_t        value1;

    rar_vm_getter_t getter2;
    rar_vm_setter_t setter2;
    uint32_t        value2;

    uint8_t instruction;
    uint8_t bytemode;
    uint8_t mode1;
    uint8_t mode2;

#if UINTPTR_MAX == UINT64_MAX
    uint8_t padding[12]; /* pad to 64 bytes on 64-bit */
#endif
};

/* Compiled filter program */
typedef struct
{
    rar_vm_opcode_t *opcodes;
    int              num_opcodes;
    int              opcodes_capacity;

    uint8_t *static_data;
    int      static_data_len;

    uint8_t *global_backup;
    int      global_backup_len;
    int      global_backup_capacity;

    uint64_t fingerprint;
} rar_vm_program_t;

/* Filter invocation */
typedef struct
{
    rar_vm_program_t *program;

    uint32_t initial_registers[8];

    uint8_t *global_data;
    int      global_data_len;
    int      global_data_capacity;
} rar_vm_invocation_t;

/* --- VM Core API --- */

void rar_vm_init(rar_vm_t *vm);
bool rar_vm_execute(rar_vm_t *vm, rar_vm_opcode_t *opcodes, int num_opcodes);

/* --- Opcode Setup --- */

void rar_vm_set_instruction(rar_vm_opcode_t *op, unsigned int instruction, bool bytemode);
void rar_vm_set_operand1(rar_vm_opcode_t *op, unsigned int mode, uint32_t value);
void rar_vm_set_operand2(rar_vm_opcode_t *op, unsigned int mode, uint32_t value);
bool rar_vm_is_terminated(rar_vm_opcode_t *opcodes, int num_opcodes);
bool rar_vm_prepare_opcodes(rar_vm_opcode_t *opcodes, int num_opcodes);

/* --- Instruction Properties --- */

int  rar_vm_num_operands(unsigned int instruction);
bool rar_vm_has_bytemode(unsigned int instruction);
bool rar_vm_is_unconditional_jump(unsigned int instruction);
bool rar_vm_is_relative_jump(unsigned int instruction);

/* --- Program API --- */

rar_vm_program_t *rar_vm_program_create(const uint8_t *bytecode, int length);
void              rar_vm_program_free(rar_vm_program_t *prog);

/* --- Invocation API --- */

rar_vm_invocation_t *rar_vm_invocation_create(rar_vm_program_t *prog, const uint8_t *global_data, int global_data_len,
                                              const uint32_t *registers);
void                 rar_vm_invocation_free(rar_vm_invocation_t *inv);
bool                 rar_vm_invocation_execute(rar_vm_invocation_t *inv, rar_vm_t *vm);
void                 rar_vm_invocation_set_global32(rar_vm_invocation_t *inv, int offset, uint32_t value);
void                 rar_vm_invocation_backup_global(rar_vm_invocation_t *inv);
void                 rar_vm_invocation_restore_global(rar_vm_invocation_t *inv);

/* --- Helper: read variable-length VM number from bitstream --- */

uint32_t rar_vm_read_number(rar_bitstream_t *bs);

/* --- Inline helpers --- */

static inline uint32_t rar_vm_read32(const uint8_t *b)
{ return ((uint32_t)b[3] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[0]; }

static inline void rar_vm_write32(uint8_t *b, uint32_t n)
{
    b[0] = n & 0xff;
    b[1] = (n >> 8) & 0xff;
    b[2] = (n >> 16) & 0xff;
    b[3] = (n >> 24) & 0xff;
}

static inline uint32_t rar_vm_mem_read32(rar_vm_t *vm, uint32_t addr)
{ return rar_vm_read32(&vm->memory[addr & RAR_VM_MEM_MASK]); }

static inline void rar_vm_mem_write32(rar_vm_t *vm, uint32_t addr, uint32_t val)
{ rar_vm_write32(&vm->memory[addr & RAR_VM_MEM_MASK], val); }

static inline uint8_t rar_vm_mem_read8(rar_vm_t *vm, uint32_t addr) { return vm->memory[addr & RAR_VM_MEM_MASK]; }

static inline void rar_vm_mem_write8(rar_vm_t *vm, uint32_t addr, uint8_t val)
{ vm->memory[addr & RAR_VM_MEM_MASK] = val; }

#endif /* AARU_COMPRESSION_NATIVE_RAR_VM_H */
