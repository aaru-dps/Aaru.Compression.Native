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

#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* CRC32 table (polynomial 0xEDB88320)                                */
/* ------------------------------------------------------------------ */

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832,
    0x79DCB8A4, 0xE0D5E91B, 0x97D2D988, 0x09B64C2B, 0x7EB17CBE, 0xE7B82D09, 0x90BF1D7F, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A,
    0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3,
    0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F6B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
    0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4,
    0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074,
    0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7822, 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525,
    0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615,
    0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x8065675B, 0x19060B61, 0x6E6F6E17, 0xFED41B76,
    0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6,
    0x41047A60, 0xDF60EFC3, 0xA8670955, 0x31681E6F, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
    0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7,
    0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F6, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6B70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278,
    0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330,
    0x24B4A3A6, 0xBAD03605, 0xCDD706FF, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

/* ------------------------------------------------------------------ */
/* Helper: bits remaining in bitstream                                */
/* ------------------------------------------------------------------ */

static inline size_t rar_bs_bits_remaining(const rar_bitstream_t *bs)
{
    size_t tail = (bs->byte_pos <= bs->len) ? (bs->len - bs->byte_pos) * 8 : 0;
    return (size_t)bs->bits_left + tail;
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static bool run_vm_or_get_labels(rar_vm_t *vm, rar_vm_opcode_t *opcodes, int num_opcodes, void ***instruction_labels);

static rar_vm_getter_t operand_getters_32[RAR_VM_NUM_MODES];
static rar_vm_getter_t operand_getters_8[RAR_VM_NUM_MODES];
static rar_vm_setter_t operand_setters_32[RAR_VM_NUM_MODES];
static rar_vm_setter_t operand_setters_8[RAR_VM_NUM_MODES];

static bool instruction_writes_first_operand(unsigned int instruction);
static bool instruction_writes_second_operand(unsigned int instruction);

static void parse_operand(rar_bitstream_t *bs, unsigned int *mode, uint32_t *value, bool bytemode, bool is_rel_jump,
                          int instruction_offset);

/* ================================================================== */
/* Getter functions — 32-bit                                          */
/* ================================================================== */

static uint32_t reg_get0_32(rar_vm_t *vm, uint32_t value) { return vm->registers[0]; }

static uint32_t reg_get1_32(rar_vm_t *vm, uint32_t value) { return vm->registers[1]; }

static uint32_t reg_get2_32(rar_vm_t *vm, uint32_t value) { return vm->registers[2]; }

static uint32_t reg_get3_32(rar_vm_t *vm, uint32_t value) { return vm->registers[3]; }

static uint32_t reg_get4_32(rar_vm_t *vm, uint32_t value) { return vm->registers[4]; }

static uint32_t reg_get5_32(rar_vm_t *vm, uint32_t value) { return vm->registers[5]; }

static uint32_t reg_get6_32(rar_vm_t *vm, uint32_t value) { return vm->registers[6]; }

static uint32_t reg_get7_32(rar_vm_t *vm, uint32_t value) { return vm->registers[7]; }

static uint32_t reg_ind_get0_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[0]); }

static uint32_t reg_ind_get1_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[1]); }

static uint32_t reg_ind_get2_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[2]); }

static uint32_t reg_ind_get3_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[3]); }

static uint32_t reg_ind_get4_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[4]); }

static uint32_t reg_ind_get5_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[5]); }

static uint32_t reg_ind_get6_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[6]); }

static uint32_t reg_ind_get7_32(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read32(vm, vm->registers[7]); }

static uint32_t idx_abs_get0_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[0]); }

static uint32_t idx_abs_get1_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[1]); }

static uint32_t idx_abs_get2_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[2]); }

static uint32_t idx_abs_get3_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[3]); }

static uint32_t idx_abs_get4_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[4]); }

static uint32_t idx_abs_get5_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[5]); }

static uint32_t idx_abs_get6_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[6]); }

static uint32_t idx_abs_get7_32(rar_vm_t *vm, uint32_t value)
{ return rar_vm_mem_read32(vm, value + vm->registers[7]); }

/* Absolute addressing is pre-masked in rar_vm_prepare_opcodes. */
static uint32_t abs_get_32(rar_vm_t *vm, uint32_t value) { return rar_vm_read32(&vm->memory[value]); }

static uint32_t imm_get(rar_vm_t *vm, uint32_t value) { return value; }

/* ================================================================== */
/* Getter functions — 8-bit                                           */
/* ================================================================== */

static uint32_t reg_get0_8(rar_vm_t *vm, uint32_t value) { return vm->registers[0] & 0xff; }

static uint32_t reg_get1_8(rar_vm_t *vm, uint32_t value) { return vm->registers[1] & 0xff; }

static uint32_t reg_get2_8(rar_vm_t *vm, uint32_t value) { return vm->registers[2] & 0xff; }

static uint32_t reg_get3_8(rar_vm_t *vm, uint32_t value) { return vm->registers[3] & 0xff; }

static uint32_t reg_get4_8(rar_vm_t *vm, uint32_t value) { return vm->registers[4] & 0xff; }

static uint32_t reg_get5_8(rar_vm_t *vm, uint32_t value) { return vm->registers[5] & 0xff; }

static uint32_t reg_get6_8(rar_vm_t *vm, uint32_t value) { return vm->registers[6] & 0xff; }

static uint32_t reg_get7_8(rar_vm_t *vm, uint32_t value) { return vm->registers[7] & 0xff; }

static uint32_t reg_ind_get0_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[0]); }

static uint32_t reg_ind_get1_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[1]); }

static uint32_t reg_ind_get2_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[2]); }

static uint32_t reg_ind_get3_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[3]); }

static uint32_t reg_ind_get4_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[4]); }

static uint32_t reg_ind_get5_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[5]); }

static uint32_t reg_ind_get6_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[6]); }

static uint32_t reg_ind_get7_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, vm->registers[7]); }

static uint32_t idx_abs_get0_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[0]); }

static uint32_t idx_abs_get1_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[1]); }

static uint32_t idx_abs_get2_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[2]); }

static uint32_t idx_abs_get3_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[3]); }

static uint32_t idx_abs_get4_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[4]); }

static uint32_t idx_abs_get5_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[5]); }

static uint32_t idx_abs_get6_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[6]); }

static uint32_t idx_abs_get7_8(rar_vm_t *vm, uint32_t value) { return rar_vm_mem_read8(vm, value + vm->registers[7]); }

static uint32_t abs_get_8(rar_vm_t *vm, uint32_t value) { return vm->memory[value]; }

/* ================================================================== */
/* Setter functions — 32-bit                                          */
/* ================================================================== */

static void reg_set0_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[0] = data; }

static void reg_set1_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[1] = data; }

static void reg_set2_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[2] = data; }

static void reg_set3_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[3] = data; }

static void reg_set4_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[4] = data; }

static void reg_set5_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[5] = data; }

static void reg_set6_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[6] = data; }

static void reg_set7_32(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[7] = data; }

static void reg_ind_set0_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[0], data); }

static void reg_ind_set1_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[1], data); }

static void reg_ind_set2_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[2], data); }

static void reg_ind_set3_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[3], data); }

static void reg_ind_set4_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[4], data); }

static void reg_ind_set5_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[5], data); }

static void reg_ind_set6_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[6], data); }

static void reg_ind_set7_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, vm->registers[7], data); }

static void idx_abs_set0_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[0], data); }

static void idx_abs_set1_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[1], data); }

static void idx_abs_set2_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[2], data); }

static void idx_abs_set3_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[3], data); }

static void idx_abs_set4_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[4], data); }

static void idx_abs_set5_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[5], data); }

static void idx_abs_set6_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[6], data); }

static void idx_abs_set7_32(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write32(vm, value + vm->registers[7], data); }

/* Absolute addressing is pre-masked in rar_vm_prepare_opcodes. */
static void abs_set_32(rar_vm_t *vm, uint32_t value, uint32_t data) { rar_vm_write32(&vm->memory[value], data); }

/* ================================================================== */
/* Setter functions — 8-bit                                           */
/* ================================================================== */

static void reg_set0_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[0] = data & 0xff; }

static void reg_set1_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[1] = data & 0xff; }

static void reg_set2_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[2] = data & 0xff; }

static void reg_set3_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[3] = data & 0xff; }

static void reg_set4_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[4] = data & 0xff; }

static void reg_set5_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[5] = data & 0xff; }

static void reg_set6_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[6] = data & 0xff; }

static void reg_set7_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->registers[7] = data & 0xff; }

static void reg_ind_set0_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[0], (uint8_t)data); }

static void reg_ind_set1_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[1], (uint8_t)data); }

static void reg_ind_set2_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[2], (uint8_t)data); }

static void reg_ind_set3_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[3], (uint8_t)data); }

static void reg_ind_set4_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[4], (uint8_t)data); }

static void reg_ind_set5_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[5], (uint8_t)data); }

static void reg_ind_set6_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[6], (uint8_t)data); }

static void reg_ind_set7_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, vm->registers[7], (uint8_t)data); }

static void idx_abs_set0_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[0], (uint8_t)data); }

static void idx_abs_set1_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[1], (uint8_t)data); }

static void idx_abs_set2_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[2], (uint8_t)data); }

static void idx_abs_set3_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[3], (uint8_t)data); }

static void idx_abs_set4_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[4], (uint8_t)data); }

static void idx_abs_set5_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[5], (uint8_t)data); }

static void idx_abs_set6_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[6], (uint8_t)data); }

static void idx_abs_set7_8(rar_vm_t *vm, uint32_t value, uint32_t data)
{ rar_vm_mem_write8(vm, value + vm->registers[7], (uint8_t)data); }

static void abs_set_8(rar_vm_t *vm, uint32_t value, uint32_t data) { vm->memory[value] = (uint8_t)data; }

/* ================================================================== */
/* Getter/setter lookup tables                                        */
/* ================================================================== */

static rar_vm_getter_t operand_getters_32[RAR_VM_NUM_MODES] = {
    [RAR_VM_REG0 + 0] = reg_get0_32,         [RAR_VM_REG0 + 1] = reg_get1_32,
    [RAR_VM_REG0 + 2] = reg_get2_32,         [RAR_VM_REG0 + 3] = reg_get3_32,
    [RAR_VM_REG0 + 4] = reg_get4_32,         [RAR_VM_REG0 + 5] = reg_get5_32,
    [RAR_VM_REG0 + 6] = reg_get6_32,         [RAR_VM_REG0 + 7] = reg_get7_32,
    [RAR_VM_REG_IND0 + 0] = reg_ind_get0_32, [RAR_VM_REG_IND0 + 1] = reg_ind_get1_32,
    [RAR_VM_REG_IND0 + 2] = reg_ind_get2_32, [RAR_VM_REG_IND0 + 3] = reg_ind_get3_32,
    [RAR_VM_REG_IND0 + 4] = reg_ind_get4_32, [RAR_VM_REG_IND0 + 5] = reg_ind_get5_32,
    [RAR_VM_REG_IND0 + 6] = reg_ind_get6_32, [RAR_VM_REG_IND0 + 7] = reg_ind_get7_32,
    [RAR_VM_IDX_ABS0 + 0] = idx_abs_get0_32, [RAR_VM_IDX_ABS0 + 1] = idx_abs_get1_32,
    [RAR_VM_IDX_ABS0 + 2] = idx_abs_get2_32, [RAR_VM_IDX_ABS0 + 3] = idx_abs_get3_32,
    [RAR_VM_IDX_ABS0 + 4] = idx_abs_get4_32, [RAR_VM_IDX_ABS0 + 5] = idx_abs_get5_32,
    [RAR_VM_IDX_ABS0 + 6] = idx_abs_get6_32, [RAR_VM_IDX_ABS0 + 7] = idx_abs_get7_32,
    [RAR_VM_ABSOLUTE] = abs_get_32,          [RAR_VM_IMMEDIATE] = imm_get,
};

static rar_vm_getter_t operand_getters_8[RAR_VM_NUM_MODES] = {
    [RAR_VM_REG0 + 0] = reg_get0_8,         [RAR_VM_REG0 + 1] = reg_get1_8,
    [RAR_VM_REG0 + 2] = reg_get2_8,         [RAR_VM_REG0 + 3] = reg_get3_8,
    [RAR_VM_REG0 + 4] = reg_get4_8,         [RAR_VM_REG0 + 5] = reg_get5_8,
    [RAR_VM_REG0 + 6] = reg_get6_8,         [RAR_VM_REG0 + 7] = reg_get7_8,
    [RAR_VM_REG_IND0 + 0] = reg_ind_get0_8, [RAR_VM_REG_IND0 + 1] = reg_ind_get1_8,
    [RAR_VM_REG_IND0 + 2] = reg_ind_get2_8, [RAR_VM_REG_IND0 + 3] = reg_ind_get3_8,
    [RAR_VM_REG_IND0 + 4] = reg_ind_get4_8, [RAR_VM_REG_IND0 + 5] = reg_ind_get5_8,
    [RAR_VM_REG_IND0 + 6] = reg_ind_get6_8, [RAR_VM_REG_IND0 + 7] = reg_ind_get7_8,
    [RAR_VM_IDX_ABS0 + 0] = idx_abs_get0_8, [RAR_VM_IDX_ABS0 + 1] = idx_abs_get1_8,
    [RAR_VM_IDX_ABS0 + 2] = idx_abs_get2_8, [RAR_VM_IDX_ABS0 + 3] = idx_abs_get3_8,
    [RAR_VM_IDX_ABS0 + 4] = idx_abs_get4_8, [RAR_VM_IDX_ABS0 + 5] = idx_abs_get5_8,
    [RAR_VM_IDX_ABS0 + 6] = idx_abs_get6_8, [RAR_VM_IDX_ABS0 + 7] = idx_abs_get7_8,
    [RAR_VM_ABSOLUTE] = abs_get_8,          [RAR_VM_IMMEDIATE] = imm_get,
};

static rar_vm_setter_t operand_setters_32[RAR_VM_NUM_MODES] = {
    [RAR_VM_REG0 + 0] = reg_set0_32,         [RAR_VM_REG0 + 1] = reg_set1_32,
    [RAR_VM_REG0 + 2] = reg_set2_32,         [RAR_VM_REG0 + 3] = reg_set3_32,
    [RAR_VM_REG0 + 4] = reg_set4_32,         [RAR_VM_REG0 + 5] = reg_set5_32,
    [RAR_VM_REG0 + 6] = reg_set6_32,         [RAR_VM_REG0 + 7] = reg_set7_32,
    [RAR_VM_REG_IND0 + 0] = reg_ind_set0_32, [RAR_VM_REG_IND0 + 1] = reg_ind_set1_32,
    [RAR_VM_REG_IND0 + 2] = reg_ind_set2_32, [RAR_VM_REG_IND0 + 3] = reg_ind_set3_32,
    [RAR_VM_REG_IND0 + 4] = reg_ind_set4_32, [RAR_VM_REG_IND0 + 5] = reg_ind_set5_32,
    [RAR_VM_REG_IND0 + 6] = reg_ind_set6_32, [RAR_VM_REG_IND0 + 7] = reg_ind_set7_32,
    [RAR_VM_IDX_ABS0 + 0] = idx_abs_set0_32, [RAR_VM_IDX_ABS0 + 1] = idx_abs_set1_32,
    [RAR_VM_IDX_ABS0 + 2] = idx_abs_set2_32, [RAR_VM_IDX_ABS0 + 3] = idx_abs_set3_32,
    [RAR_VM_IDX_ABS0 + 4] = idx_abs_set4_32, [RAR_VM_IDX_ABS0 + 5] = idx_abs_set5_32,
    [RAR_VM_IDX_ABS0 + 6] = idx_abs_set6_32, [RAR_VM_IDX_ABS0 + 7] = idx_abs_set7_32,
    [RAR_VM_ABSOLUTE] = abs_set_32,
};

static rar_vm_setter_t operand_setters_8[RAR_VM_NUM_MODES] = {
    [RAR_VM_REG0 + 0] = reg_set0_8,         [RAR_VM_REG0 + 1] = reg_set1_8,
    [RAR_VM_REG0 + 2] = reg_set2_8,         [RAR_VM_REG0 + 3] = reg_set3_8,
    [RAR_VM_REG0 + 4] = reg_set4_8,         [RAR_VM_REG0 + 5] = reg_set5_8,
    [RAR_VM_REG0 + 6] = reg_set6_8,         [RAR_VM_REG0 + 7] = reg_set7_8,
    [RAR_VM_REG_IND0 + 0] = reg_ind_set0_8, [RAR_VM_REG_IND0 + 1] = reg_ind_set1_8,
    [RAR_VM_REG_IND0 + 2] = reg_ind_set2_8, [RAR_VM_REG_IND0 + 3] = reg_ind_set3_8,
    [RAR_VM_REG_IND0 + 4] = reg_ind_set4_8, [RAR_VM_REG_IND0 + 5] = reg_ind_set5_8,
    [RAR_VM_REG_IND0 + 6] = reg_ind_set6_8, [RAR_VM_REG_IND0 + 7] = reg_ind_set7_8,
    [RAR_VM_IDX_ABS0 + 0] = idx_abs_set0_8, [RAR_VM_IDX_ABS0 + 1] = idx_abs_set1_8,
    [RAR_VM_IDX_ABS0 + 2] = idx_abs_set2_8, [RAR_VM_IDX_ABS0 + 3] = idx_abs_set3_8,
    [RAR_VM_IDX_ABS0 + 4] = idx_abs_set4_8, [RAR_VM_IDX_ABS0 + 5] = idx_abs_set5_8,
    [RAR_VM_IDX_ABS0 + 6] = idx_abs_set6_8, [RAR_VM_IDX_ABS0 + 7] = idx_abs_set7_8,
    [RAR_VM_ABSOLUTE] = abs_set_8,
};

/* ================================================================== */
/* Instruction flags table                                            */
/* ================================================================== */

#define F_0OP    0
#define F_1OP    1
#define F_2OP    2
#define F_OPMASK 3
#define F_BYTE   4
#define F_UJMP   8
#define F_RJMP   16
#define F_WR1    32
#define F_WR2    64
#define F_RDST   128
#define F_WRST   256

static const int instruction_flags[RAR_VM_NUM_INSTRUCTIONS] = {
    [RAR_VM_MOV]   = F_2OP | F_BYTE | F_WR1,
    [RAR_VM_CMP]   = F_2OP | F_BYTE | F_WRST,
    [RAR_VM_ADD]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_SUB]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_JZ]    = F_1OP | F_UJMP | F_RJMP | F_RDST,
    [RAR_VM_JNZ]   = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_INC]   = F_1OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_DEC]   = F_1OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_JMP]   = F_1OP | F_RJMP,
    [RAR_VM_XOR]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_AND]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_OR]    = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_TEST]  = F_2OP | F_BYTE | F_WRST,
    [RAR_VM_JS]    = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_JNS]   = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_JB]    = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_JBE]   = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_JA]    = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_JAE]   = F_1OP | F_RJMP | F_RDST,
    [RAR_VM_PUSH]  = F_1OP,
    [RAR_VM_POP]   = F_1OP,
    [RAR_VM_CALL]  = F_1OP | F_RJMP,
    [RAR_VM_RET]   = F_0OP | F_UJMP,
    [RAR_VM_NOT]   = F_1OP | F_BYTE | F_WR1,
    [RAR_VM_SHL]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_SHR]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_SAR]   = F_2OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_NEG]   = F_1OP | F_BYTE | F_WR1 | F_WRST,
    [RAR_VM_PUSHA] = F_0OP,
    [RAR_VM_POPA]  = F_0OP,
    [RAR_VM_PUSHF] = F_0OP | F_RDST,
    [RAR_VM_POPF]  = F_0OP | F_WRST,
    [RAR_VM_MOVZX] = F_2OP | F_WR1,
    [RAR_VM_MOVSX] = F_2OP | F_WR1,
    [RAR_VM_XCHG]  = F_2OP | F_WR1 | F_WR2 | F_BYTE,
    [RAR_VM_MUL]   = F_2OP | F_BYTE | F_WR1,
    [RAR_VM_DIV]   = F_2OP | F_BYTE | F_WR1,
    [RAR_VM_ADC]   = F_2OP | F_BYTE | F_WR1 | F_RDST | F_WRST,
    [RAR_VM_SBB]   = F_2OP | F_BYTE | F_WR1 | F_RDST | F_WRST,
    [RAR_VM_PRINT] = F_0OP,
};

/* ================================================================== */
/* Instruction properties                                             */
/* ================================================================== */

int rar_vm_num_operands(unsigned int instruction)
{
    if(instruction >= RAR_VM_NUM_INSTRUCTIONS) return 0;
    return instruction_flags[instruction] & F_OPMASK;
}

bool rar_vm_has_bytemode(unsigned int instruction)
{
    if(instruction >= RAR_VM_NUM_INSTRUCTIONS) return false;
    return (instruction_flags[instruction] & F_BYTE) != 0;
}

bool rar_vm_is_unconditional_jump(unsigned int instruction)
{
    if(instruction >= RAR_VM_NUM_INSTRUCTIONS) return false;
    return (instruction_flags[instruction] & F_UJMP) != 0;
}

bool rar_vm_is_relative_jump(unsigned int instruction)
{
    if(instruction >= RAR_VM_NUM_INSTRUCTIONS) return false;
    return (instruction_flags[instruction] & F_RJMP) != 0;
}

static bool instruction_writes_first_operand(unsigned int instruction)
{
    if(instruction >= RAR_VM_NUM_INSTRUCTIONS) return false;
    return (instruction_flags[instruction] & F_WR1) != 0;
}

static bool instruction_writes_second_operand(unsigned int instruction)
{
    if(instruction >= RAR_VM_NUM_INSTRUCTIONS) return false;
    return (instruction_flags[instruction] & F_WR2) != 0;
}

/* ================================================================== */
/* VM init                                                            */
/* ================================================================== */

void rar_vm_init(rar_vm_t *vm) { memset(vm->registers, 0, sizeof(vm->registers)); }

/* ================================================================== */
/* Opcode setup                                                       */
/* ================================================================== */

void rar_vm_set_instruction(rar_vm_opcode_t *op, unsigned int instruction, bool bytemode)
{
    op->instruction = (uint8_t)instruction;
    op->bytemode    = (uint8_t)bytemode;
}

void rar_vm_set_operand1(rar_vm_opcode_t *op, unsigned int mode, uint32_t value)
{
    op->mode1  = (uint8_t)mode;
    op->value1 = value;
}

void rar_vm_set_operand2(rar_vm_opcode_t *op, unsigned int mode, uint32_t value)
{
    op->mode2  = (uint8_t)mode;
    op->value2 = value;
}

bool rar_vm_is_terminated(rar_vm_opcode_t *opcodes, int num_opcodes)
{
    if(num_opcodes == 0) return false;
    return rar_vm_is_unconditional_jump(opcodes[num_opcodes - 1].instruction);
}

bool rar_vm_prepare_opcodes(rar_vm_opcode_t *opcodes, int num_opcodes)
{
    void **labels_32, **labels_8;

    run_vm_or_get_labels(NULL, NULL, 0, &labels_32);
    labels_8 = &labels_32[RAR_VM_NUM_INSTRUCTIONS];

    for(int i = 0; i < num_opcodes; i++)
    {
        if(opcodes[i].instruction >= RAR_VM_NUM_INSTRUCTIONS) return false;

        void           **labels;
        rar_vm_setter_t *setters;
        rar_vm_getter_t *getters;

        if(opcodes[i].instruction == RAR_VM_MOVSX || opcodes[i].instruction == RAR_VM_MOVZX)
        {
            labels  = labels_32;
            getters = operand_getters_8;
            setters = operand_setters_32;
        }
        else if(opcodes[i].bytemode)
        {
            if(!rar_vm_has_bytemode(opcodes[i].instruction)) return false;

            labels  = labels_8;
            getters = operand_getters_8;
            setters = operand_setters_8;
        }
        else
        {
            labels  = labels_32;
            getters = operand_getters_32;
            setters = operand_setters_32;
        }

        opcodes[i].label = labels[opcodes[i].instruction];

        int numops = rar_vm_num_operands(opcodes[i].instruction);

        if(numops >= 1)
        {
            if(opcodes[i].mode1 >= RAR_VM_NUM_MODES) return false;
            opcodes[i].getter1 = getters[opcodes[i].mode1];
            opcodes[i].setter1 = setters[opcodes[i].mode1];

            if(opcodes[i].mode1 == RAR_VM_IMMEDIATE)
            {
                if(instruction_writes_first_operand(opcodes[i].instruction)) return false;
            }
            else if(opcodes[i].mode1 == RAR_VM_ABSOLUTE) { opcodes[i].value1 &= RAR_VM_MEM_MASK; }
        }

        if(numops == 2)
        {
            if(opcodes[i].mode2 >= RAR_VM_NUM_MODES) return false;
            opcodes[i].getter2 = getters[opcodes[i].mode2];
            opcodes[i].setter2 = setters[opcodes[i].mode2];

            if(opcodes[i].mode2 == RAR_VM_IMMEDIATE)
            {
                if(instruction_writes_second_operand(opcodes[i].instruction)) return false;
            }
            else if(opcodes[i].mode2 == RAR_VM_ABSOLUTE) { opcodes[i].value2 &= RAR_VM_MEM_MASK; }
        }
    }

    return true;
}

/* ================================================================== */
/* Computed-goto threaded interpreter                                  */
/* ================================================================== */

#define CarryFlag 1
#define ZeroFlag  2
#define SignFlag  0x80000000

#define SignExtend(a) ((uint32_t)((int8_t)(a)))

#define GetOperand1()     (opcode->getter1(vm, opcode->value1))
#define GetOperand2()     (opcode->getter2(vm, opcode->value2))
#define SetOperand1(data) opcode->setter1(vm, opcode->value1, data)
#define SetOperand2(data) opcode->setter2(vm, opcode->value2, data)

#define SetFlagsWithCarry(res, carry)                                               \
    ({                                                                              \
        uint32_t result = (res);                                                    \
        flags           = (result == 0 ? ZeroFlag : (result & SignFlag)) | (carry); \
    })
#define SetByteFlagsWithCarry(res, carry)                                                       \
    ({                                                                                          \
        uint32_t result = (res);                                                                \
        flags           = (result == 0 ? ZeroFlag : (SignExtend(result) & SignFlag)) | (carry); \
    })
#define SetFlags(res) (SetFlagsWithCarry(res, 0))

#define SetOperand1AndFlagsWithCarry(res, carry) \
    ({                                           \
        uint32_t r = (res);                      \
        SetFlagsWithCarry(r, carry);             \
        SetOperand1(r);                          \
    })
#define SetOperand1AndByteFlagsWithCarry(res, carry) \
    ({                                               \
        uint32_t r = (res);                          \
        SetByteFlagsWithCarry(r, carry);             \
        SetOperand1(r);                              \
    })
#define SetOperand1AndFlags(res) \
    ({                           \
        uint32_t r = (res);      \
        SetFlags(r);             \
        SetOperand1(r);          \
    })

#define Debug() ({})

#define NextInstruction()     \
    ({                        \
        opcode++;             \
        Debug();              \
        goto * opcode->label; \
    })
#define Jump(offs)                                   \
    ({                                               \
        uint32_t o = (offs);                         \
        if(o >= (uint32_t)num_opcodes) return false; \
        opcode = &opcodes[o];                        \
        Debug();                                     \
        goto * opcode->label;                        \
    })

static bool run_vm_or_get_labels(rar_vm_t *vm, rar_vm_opcode_t *opcodes, int num_opcodes, void ***instruction_labels)
{
    static void *labels[2][RAR_VM_NUM_INSTRUCTIONS] = {
        [0][RAR_VM_MOV] = &&MovLabel,     [1][RAR_VM_MOV] = &&MovLabel,     [0][RAR_VM_CMP] = &&CmpLabel,
        [1][RAR_VM_CMP] = &&CmpLabel,     [0][RAR_VM_ADD] = &&AddLabel,     [1][RAR_VM_ADD] = &&AddByteLabel,
        [0][RAR_VM_SUB] = &&SubLabel,     [1][RAR_VM_SUB] = &&SubLabel,     [0][RAR_VM_JZ] = &&JzLabel,
        [0][RAR_VM_JNZ] = &&JnzLabel,     [0][RAR_VM_INC] = &&IncLabel,     [1][RAR_VM_INC] = &&IncByteLabel,
        [0][RAR_VM_DEC] = &&DecLabel,     [1][RAR_VM_DEC] = &&DecByteLabel, [0][RAR_VM_JMP] = &&JmpLabel,
        [0][RAR_VM_XOR] = &&XorLabel,     [1][RAR_VM_XOR] = &&XorLabel,     [0][RAR_VM_AND] = &&AndLabel,
        [1][RAR_VM_AND] = &&AndLabel,     [0][RAR_VM_OR] = &&OrLabel,       [1][RAR_VM_OR] = &&OrLabel,
        [0][RAR_VM_TEST] = &&TestLabel,   [1][RAR_VM_TEST] = &&TestLabel,   [0][RAR_VM_JS] = &&JsLabel,
        [0][RAR_VM_JNS] = &&JnsLabel,     [0][RAR_VM_JB] = &&JbLabel,       [0][RAR_VM_JBE] = &&JbeLabel,
        [0][RAR_VM_JA] = &&JaLabel,       [0][RAR_VM_JAE] = &&JaeLabel,     [0][RAR_VM_PUSH] = &&PushLabel,
        [0][RAR_VM_POP] = &&PopLabel,     [0][RAR_VM_CALL] = &&CallLabel,   [0][RAR_VM_RET] = &&RetLabel,
        [0][RAR_VM_NOT] = &&NotLabel,     [1][RAR_VM_NOT] = &&NotLabel,     [0][RAR_VM_SHL] = &&ShlLabel,
        [1][RAR_VM_SHL] = &&ShlLabel,     [0][RAR_VM_SHR] = &&ShrLabel,     [1][RAR_VM_SHR] = &&ShrLabel,
        [0][RAR_VM_SAR] = &&SarLabel,     [1][RAR_VM_SAR] = &&SarLabel,     [0][RAR_VM_NEG] = &&NegLabel,
        [1][RAR_VM_NEG] = &&NegLabel,     [0][RAR_VM_PUSHA] = &&PushaLabel, [0][RAR_VM_POPA] = &&PopaLabel,
        [0][RAR_VM_PUSHF] = &&PushfLabel, [0][RAR_VM_POPF] = &&PopfLabel,   [0][RAR_VM_MOVZX] = &&MovzxLabel,
        [0][RAR_VM_MOVSX] = &&MovsxLabel, [0][RAR_VM_XCHG] = &&XchgLabel,   [0][RAR_VM_MUL] = &&MulLabel,
        [1][RAR_VM_MUL] = &&MulLabel,     [0][RAR_VM_DIV] = &&DivLabel,     [1][RAR_VM_DIV] = &&DivLabel,
        [0][RAR_VM_ADC] = &&AdcLabel,     [1][RAR_VM_ADC] = &&AdcByteLabel, [0][RAR_VM_SBB] = &&SbbLabel,
        [1][RAR_VM_SBB] = &&SbbByteLabel, [0][RAR_VM_PRINT] = &&PrintLabel,
    };

    if(instruction_labels)
    {
        *instruction_labels = &labels[0][0];
        return true;
    }

    rar_vm_opcode_t *opcode;
    uint32_t         flags = vm->flags;

    Jump(0);

MovLabel:
    SetOperand1(GetOperand2());
    NextInstruction();

CmpLabel:
{
    uint32_t term1 = GetOperand1();
    SetFlagsWithCarry(term1 - GetOperand2(), result > term1);
    NextInstruction();
}

AddLabel:
{
    uint32_t term1 = GetOperand1();
    SetOperand1AndFlagsWithCarry(term1 + GetOperand2(), result < term1);
    NextInstruction();
}
AddByteLabel:
{
    uint32_t term1 = GetOperand1();
    SetOperand1AndByteFlagsWithCarry(term1 + GetOperand2() & 0xff, result < term1);
    NextInstruction();
}

SubLabel:
{
    uint32_t term1 = GetOperand1();
    SetOperand1AndFlagsWithCarry(term1 - GetOperand2(), result > term1);
    NextInstruction();
}

JzLabel:
    if(flags & ZeroFlag)
        Jump(GetOperand1());
    else
        NextInstruction();

JnzLabel:
    if(!(flags & ZeroFlag))
        Jump(GetOperand1());
    else
        NextInstruction();

IncLabel:
    SetOperand1AndFlags(GetOperand1() + 1);
    NextInstruction();
IncByteLabel:
    SetOperand1AndFlags(GetOperand1() + 1 & 0xff);
    NextInstruction();

DecLabel:
    SetOperand1AndFlags(GetOperand1() - 1);
    NextInstruction();
DecByteLabel:
    SetOperand1AndFlags(GetOperand1() - 1 & 0xff);
    NextInstruction();

JmpLabel:
    Jump(GetOperand1());

XorLabel:
    SetOperand1AndFlags(GetOperand1() ^ GetOperand2());
    NextInstruction();

AndLabel:
    SetOperand1AndFlags(GetOperand1() & GetOperand2());
    NextInstruction();

OrLabel:
    SetOperand1AndFlags(GetOperand1() | GetOperand2());
    NextInstruction();

TestLabel:
    SetFlags(GetOperand1() & GetOperand2());
    NextInstruction();

JsLabel:
    if(flags & SignFlag)
        Jump(GetOperand1());
    else
        NextInstruction();

JnsLabel:
    if(!(flags & SignFlag))
        Jump(GetOperand1());
    else
        NextInstruction();

JbLabel:
    if(flags & CarryFlag)
        Jump(GetOperand1());
    else
        NextInstruction();

JbeLabel:
    if(flags & (CarryFlag | ZeroFlag))
        Jump(GetOperand1());
    else
        NextInstruction();

JaLabel:
    if(!(flags & (CarryFlag | ZeroFlag)))
        Jump(GetOperand1());
    else
        NextInstruction();

JaeLabel:
    if(!(flags & CarryFlag))
        Jump(GetOperand1());
    else
        NextInstruction();

PushLabel:
    vm->registers[7] -= 4;
    rar_vm_mem_write32(vm, vm->registers[7], GetOperand1());
    NextInstruction();

PopLabel:
    SetOperand1(rar_vm_mem_read32(vm, vm->registers[7]));
    vm->registers[7] += 4;
    NextInstruction();

CallLabel:
    vm->registers[7] -= 4;
    rar_vm_mem_write32(vm, vm->registers[7], (uint32_t)(opcode - opcodes + 1));
    Jump(GetOperand1());

RetLabel:
{
    if(vm->registers[7] >= RAR_VM_MEM_SIZE)
    {
        vm->flags = flags;
        return true;
    }
    uint32_t retaddr = rar_vm_mem_read32(vm, vm->registers[7]);
    vm->registers[7] += 4;
    Jump(retaddr);
}

NotLabel:
    SetOperand1(~GetOperand1());
    NextInstruction();

ShlLabel:
{
    uint32_t op1 = GetOperand1();
    uint32_t op2 = GetOperand2();
    SetOperand1AndFlagsWithCarry(op1 << op2, ((op1 << (op2 - 1)) & 0x80000000) != 0);
    NextInstruction();
}

ShrLabel:
{
    uint32_t op1 = GetOperand1();
    uint32_t op2 = GetOperand2();
    SetOperand1AndFlagsWithCarry(op1 >> op2, ((op1 >> (op2 - 1)) & 1) != 0);
    NextInstruction();
}

SarLabel:
{
    uint32_t op1 = GetOperand1();
    uint32_t op2 = GetOperand2();
    SetOperand1AndFlagsWithCarry(((int32_t)op1) >> op2, ((op1 >> (op2 - 1)) & 1) != 0);
    NextInstruction();
}

NegLabel:
    SetOperand1AndFlagsWithCarry(-GetOperand1(), result != 0);
    NextInstruction();

PushaLabel:
    for(int i = 0; i < 8; i++) rar_vm_mem_write32(vm, vm->registers[7] - 4 - i * 4, vm->registers[i]);
    vm->registers[7] -= 32;
    NextInstruction();

PopaLabel:
    for(int i = 0; i < 8; i++) vm->registers[i] = rar_vm_mem_read32(vm, vm->registers[7] + 28 - i * 4);
    NextInstruction();

PushfLabel:
    vm->registers[7] -= 4;
    rar_vm_mem_write32(vm, vm->registers[7], flags);
    NextInstruction();

PopfLabel:
    flags = rar_vm_mem_read32(vm, vm->registers[7]);
    vm->registers[7] += 4;
    NextInstruction();

MovzxLabel:
    SetOperand1(GetOperand2());
    NextInstruction();

MovsxLabel:
    SetOperand1(SignExtend(GetOperand2()));
    NextInstruction();

XchgLabel:
{
    uint32_t op1 = GetOperand1();
    uint32_t op2 = GetOperand2();
    SetOperand1(op2);
    SetOperand2(op1);
    NextInstruction();
}

MulLabel:
    SetOperand1(GetOperand1() * GetOperand2());
    NextInstruction();

DivLabel:
{
    uint32_t denominator = GetOperand2();
    if(denominator != 0) SetOperand1(GetOperand1() / denominator);
    NextInstruction();
}

AdcLabel:
{
    uint32_t term1 = GetOperand1();
    uint32_t carry = flags & CarryFlag;
    SetOperand1AndFlagsWithCarry(term1 + GetOperand2() + carry, result < term1 || (result == term1 && carry));
    NextInstruction();
}
AdcByteLabel:
{
    uint32_t term1 = GetOperand1();
    uint32_t carry = flags & CarryFlag;
    SetOperand1AndFlagsWithCarry(term1 + GetOperand2() + carry & 0xff, result < term1 || (result == term1 && carry));
    NextInstruction();
}

SbbLabel:
{
    uint32_t term1 = GetOperand1();
    uint32_t carry = flags & CarryFlag;
    SetOperand1AndFlagsWithCarry(term1 - GetOperand2() - carry, result > term1 || (result == term1 && carry));
    NextInstruction();
}
SbbByteLabel:
{
    uint32_t term1 = GetOperand1();
    uint32_t carry = flags & CarryFlag;
    SetOperand1AndFlagsWithCarry(term1 - GetOperand2() - carry & 0xff, result > term1 || (result == term1 && carry));
    NextInstruction();
}

PrintLabel:
    NextInstruction();

    return false;
}

#undef CarryFlag
#undef ZeroFlag
#undef SignFlag
#undef SignExtend
#undef GetOperand1
#undef GetOperand2
#undef SetOperand1
#undef SetOperand2
#undef SetFlagsWithCarry
#undef SetByteFlagsWithCarry
#undef SetFlags
#undef SetOperand1AndFlagsWithCarry
#undef SetOperand1AndByteFlagsWithCarry
#undef SetOperand1AndFlags
#undef Debug
#undef NextInstruction
#undef Jump

/* ================================================================== */
/* Execute                                                            */
/* ================================================================== */

bool rar_vm_execute(rar_vm_t *vm, rar_vm_opcode_t *opcodes, int num_opcodes)
{
    if(!rar_vm_is_terminated(opcodes, num_opcodes)) return false;

    vm->flags = 0;

    return run_vm_or_get_labels(vm, opcodes, num_opcodes, NULL);
}

/* ================================================================== */
/* Variable-length VM number reader                                   */
/* ================================================================== */

uint32_t rar_vm_read_number(rar_bitstream_t *bs)
{
    switch(rar_bs_read_bits(bs, 2))
    {
        case 0:
            return rar_bs_read_bits(bs, 4);
        case 1:
        {
            int val = (int)rar_bs_read_bits(bs, 8);
            if(val >= 16)
                return (uint32_t)val;
            else
                return 0xffffff00 | ((uint32_t)val << 4) | rar_bs_read_bits(bs, 4);
        }
        case 2:
            return rar_bs_read_bits(bs, 16);
        default:
            return (rar_bs_read_bits(bs, 16) << 16) | rar_bs_read_bits(bs, 16);
    }
}

/* ================================================================== */
/* Operand parser                                                     */
/* ================================================================== */

static void parse_operand(rar_bitstream_t *bs, unsigned int *mode, uint32_t *value, bool bytemode, bool is_rel_jump,
                          int instruction_offset)
{
    if(rar_bs_read_bit(bs))
    {
        int reg = (int)rar_bs_read_bits(bs, 3);
        *mode   = RAR_VM_REG0 + reg;
    }
    else
    {
        if(rar_bs_read_bit(bs))
        {
            if(rar_bs_read_bit(bs))
            {
                if(rar_bs_read_bit(bs))
                {
                    *value = rar_vm_read_number(bs);
                    *mode  = RAR_VM_ABSOLUTE;
                }
                else
                {
                    int reg = (int)rar_bs_read_bits(bs, 3);
                    *value  = rar_vm_read_number(bs);
                    *mode   = RAR_VM_IDX_ABS0 + reg;
                }
            }
            else
            {
                int reg = (int)rar_bs_read_bits(bs, 3);
                *mode   = RAR_VM_REG_IND0 + reg;
            }
        }
        else
        {
            if(bytemode)
                *value = rar_bs_read_bits(bs, 8);
            else
                *value = rar_vm_read_number(bs);
            *mode = RAR_VM_IMMEDIATE;

            if(is_rel_jump)
            {
                if(*value >= 256)
                    *value -= 256;
                else
                {
                    if(*value >= 136)
                        *value -= 264;
                    else if(*value >= 16)
                        *value -= 8;
                    else if(*value >= 8)
                        *value -= 16;
                    *value += (uint32_t)instruction_offset;
                }
            }
        }
    }
}

/* ================================================================== */
/* Program: create from bytecode                                      */
/* ================================================================== */

rar_vm_program_t *rar_vm_program_create(const uint8_t *bytecode, int length)
{
    if(length == 0) return NULL;

    /* XOR checksum: bytes[0] must equal XOR of bytes[1..length-1] */
    uint8_t xor_check = 0;
    for(int i = 1; i < length; i++) xor_check ^= bytecode[i];
    if(xor_check != bytecode[0]) return NULL;

    /* Allocate program structure */
    rar_vm_program_t *prog = calloc(1, sizeof(rar_vm_program_t));
    if(!prog) return NULL;

    /* CRC32 fingerprint combined with length */
    uint32_t crc = 0xffffffff;
    for(int i = 0; i < length; i++) crc = crc32_table[(crc ^ bytecode[i]) & 0xff] ^ (crc >> 8);
    crc ^= 0xffffffff;
    prog->fingerprint = (uint64_t)crc | ((uint64_t)length << 32);

    /* Create bitstream starting at bytes[1] (skip XOR byte) */
    rar_bitstream_t bs;
    rar_bs_init(&bs, &bytecode[1], (size_t)(length - 1));

    /* Read static data if first bit is 1 */
    if(rar_bs_read_bit(&bs))
    {
        int static_len    = (int)rar_vm_read_number(&bs) + 1;
        prog->static_data = malloc((size_t)static_len);
        if(!prog->static_data)
        {
            rar_vm_program_free(prog);
            return NULL;
        }
        prog->static_data_len = static_len;
        for(int i = 0; i < static_len; i++) prog->static_data[i] = (uint8_t)rar_bs_read_bits(&bs, 8);
    }

    /* Initial capacity for opcodes */
    int capacity  = 16;
    prog->opcodes = calloc((size_t)capacity, sizeof(rar_vm_opcode_t));
    if(!prog->opcodes)
    {
        rar_vm_program_free(prog);
        return NULL;
    }
    prog->opcodes_capacity = capacity;
    prog->num_opcodes      = 0;

    /* Read instructions while at least 8 bits remain */
    while(rar_bs_bits_remaining(&bs) >= 8)
    {
        /* Grow opcodes array if needed */
        if(prog->num_opcodes >= prog->opcodes_capacity)
        {
            int              new_cap = prog->opcodes_capacity * 2;
            rar_vm_opcode_t *new_ops = realloc(prog->opcodes, (size_t)new_cap * sizeof(rar_vm_opcode_t));
            if(!new_ops)
            {
                rar_vm_program_free(prog);
                return NULL;
            }
            memset(&new_ops[prog->opcodes_capacity], 0,
                   (size_t)(new_cap - prog->opcodes_capacity) * sizeof(rar_vm_opcode_t));
            prog->opcodes          = new_ops;
            prog->opcodes_capacity = new_cap;
        }

        int curr = prog->num_opcodes;
        prog->num_opcodes++;
        rar_vm_opcode_t *op = &prog->opcodes[curr];
        memset(op, 0, sizeof(rar_vm_opcode_t));

        /* Read instruction number */
        int instruction = (int)rar_bs_read_bits(&bs, 4);
        if(instruction & 0x08) instruction = ((instruction << 2) | (int)rar_bs_read_bits(&bs, 2)) - 24;

        /* Read byte mode if supported */
        bool bytemode = false;
        if(rar_vm_has_bytemode((unsigned int)instruction)) bytemode = rar_bs_read_bit(&bs) != 0;

        rar_vm_set_instruction(op, (unsigned int)instruction, bytemode);

        int numargs = rar_vm_num_operands((unsigned int)instruction);

        if(numargs >= 1)
        {
            unsigned int mode  = 0;
            uint32_t     value = 0;
            parse_operand(&bs, &mode, &value, bytemode, rar_vm_is_relative_jump((unsigned int)instruction), curr);
            rar_vm_set_operand1(op, mode, value);
        }
        if(numargs == 2)
        {
            unsigned int mode  = 0;
            uint32_t     value = 0;
            parse_operand(&bs, &mode, &value, bytemode, false, 0);
            rar_vm_set_operand2(op, mode, value);
        }
    }

    /* Append RET if program doesn't end with an unconditional jump */
    if(!rar_vm_is_terminated(prog->opcodes, prog->num_opcodes))
    {
        if(prog->num_opcodes >= prog->opcodes_capacity)
        {
            int              new_cap = prog->opcodes_capacity + 1;
            rar_vm_opcode_t *new_ops = realloc(prog->opcodes, (size_t)new_cap * sizeof(rar_vm_opcode_t));
            if(!new_ops)
            {
                rar_vm_program_free(prog);
                return NULL;
            }
            prog->opcodes          = new_ops;
            prog->opcodes_capacity = new_cap;
        }
        rar_vm_opcode_t *op = &prog->opcodes[prog->num_opcodes];
        memset(op, 0, sizeof(rar_vm_opcode_t));
        rar_vm_set_instruction(op, RAR_VM_RET, false);
        prog->num_opcodes++;
    }

    /* Resolve labels, getters, setters */
    if(!rar_vm_prepare_opcodes(prog->opcodes, prog->num_opcodes))
    {
        rar_vm_program_free(prog);
        return NULL;
    }

    return prog;
}

/* ================================================================== */
/* Program: free                                                      */
/* ================================================================== */

void rar_vm_program_free(rar_vm_program_t *prog)
{
    if(!prog) return;
    free(prog->opcodes);
    free(prog->static_data);
    free(prog->global_backup);
    free(prog);
}

/* ================================================================== */
/* Invocation: create                                                 */
/* ================================================================== */

rar_vm_invocation_t *rar_vm_invocation_create(rar_vm_program_t *prog, const uint8_t *global_data, int global_data_len,
                                              const uint32_t *registers)
{
    rar_vm_invocation_t *inv = calloc(1, sizeof(rar_vm_invocation_t));
    if(!inv) return NULL;

    inv->program = prog;

    /* Allocate global_data, padded to at least RAR_VM_SYS_GLOBAL_SZ */
    int alloc_len = global_data_len;
    if(alloc_len < RAR_VM_SYS_GLOBAL_SZ) alloc_len = RAR_VM_SYS_GLOBAL_SZ;

    inv->global_data = calloc(1, (size_t)alloc_len);
    if(!inv->global_data)
    {
        free(inv);
        return NULL;
    }
    inv->global_data_capacity = alloc_len;

    if(global_data && global_data_len > 0)
    {
        memcpy(inv->global_data, global_data, (size_t)global_data_len);
        inv->global_data_len = alloc_len;
    }
    else
    {
        inv->global_data_len = RAR_VM_SYS_GLOBAL_SZ;
    }

    if(registers)
        memcpy(inv->initial_registers, registers, sizeof(inv->initial_registers));
    else
        memset(inv->initial_registers, 0, sizeof(inv->initial_registers));

    return inv;
}

/* ================================================================== */
/* Invocation: free                                                   */
/* ================================================================== */

void rar_vm_invocation_free(rar_vm_invocation_t *inv)
{
    if(!inv) return;
    free(inv->global_data);
    free(inv);
}

/* ================================================================== */
/* Invocation: execute on VM                                          */
/* ================================================================== */

bool rar_vm_invocation_execute(rar_vm_invocation_t *inv, rar_vm_t *vm)
{
    /* Write global_data to VM memory at system global address */
    int glob_len = inv->global_data_len;
    if(glob_len > RAR_VM_SYS_GLOBAL_SZ) glob_len = RAR_VM_SYS_GLOBAL_SZ;
    if(glob_len > 0) memcpy(&vm->memory[RAR_VM_SYS_GLOBAL], inv->global_data, (size_t)glob_len);

    /* Write static_data to VM memory at user global address */
    if(inv->program->static_data && inv->program->static_data_len > 0)
    {
        int static_len = inv->program->static_data_len;
        if(static_len > RAR_VM_USR_GLOBAL_SZ - glob_len) static_len = RAR_VM_USR_GLOBAL_SZ - glob_len;
        if(static_len > 0) memcpy(&vm->memory[RAR_VM_USR_GLOBAL], inv->program->static_data, (size_t)static_len);
    }

    /* Set registers from initial state */
    memcpy(vm->registers, inv->initial_registers, sizeof(vm->registers));

    /* Execute program opcodes */
    if(!rar_vm_execute(vm, inv->program->opcodes, inv->program->num_opcodes)) return false;

    /* Read back updated globals from VM memory */
    uint32_t new_glob_len = rar_vm_mem_read32(vm, RAR_VM_SYS_GLOBAL + 0x30);
    if(new_glob_len > RAR_VM_USR_GLOBAL_SZ) new_glob_len = RAR_VM_USR_GLOBAL_SZ;

    if(new_glob_len > 0)
    {
        int total = RAR_VM_SYS_GLOBAL_SZ + (int)new_glob_len;

        /* Resize global_data if needed */
        if(total > inv->global_data_capacity)
        {
            uint8_t *new_data = realloc(inv->global_data, (size_t)total);
            if(!new_data) return false;
            inv->global_data          = new_data;
            inv->global_data_capacity = total;
        }
        inv->global_data_len = total;
        memcpy(inv->global_data, &vm->memory[RAR_VM_SYS_GLOBAL], (size_t)total);
    }
    else
    {
        inv->global_data_len = 0;
    }

    return true;
}

/* ================================================================== */
/* Invocation: set a 32-bit value in global data                      */
/* ================================================================== */

void rar_vm_invocation_set_global32(rar_vm_invocation_t *inv, int offset, uint32_t value)
{
    if(offset < 0 || offset + 4 > inv->global_data_len) return;
    rar_vm_write32(&inv->global_data[offset], value);
}

/* ================================================================== */
/* Invocation: backup global data to program                          */
/* ================================================================== */

void rar_vm_invocation_backup_global(rar_vm_invocation_t *inv)
{
    rar_vm_program_t *prog = inv->program;

    if(inv->global_data_len > RAR_VM_SYS_GLOBAL_SZ)
    {
        /* Has user globals — back them up */
        if(inv->global_data_len > prog->global_backup_capacity)
        {
            uint8_t *new_buf = realloc(prog->global_backup, (size_t)inv->global_data_len);
            if(!new_buf) return;
            prog->global_backup          = new_buf;
            prog->global_backup_capacity = inv->global_data_len;
        }
        memcpy(prog->global_backup, inv->global_data, (size_t)inv->global_data_len);
        prog->global_backup_len = inv->global_data_len;
    }
    else
    {
        prog->global_backup_len = 0;
    }
}

/* ================================================================== */
/* Invocation: restore global data from program backup                */
/* ================================================================== */

void rar_vm_invocation_restore_global(rar_vm_invocation_t *inv)
{
    rar_vm_program_t *prog = inv->program;

    if(prog->global_backup_len > RAR_VM_SYS_GLOBAL_SZ)
    {
        if(prog->global_backup_len > inv->global_data_capacity)
        {
            uint8_t *new_buf = realloc(inv->global_data, (size_t)prog->global_backup_len);
            if(!new_buf) return;
            inv->global_data          = new_buf;
            inv->global_data_capacity = prog->global_backup_len;
        }
        memcpy(inv->global_data, prog->global_backup, (size_t)prog->global_backup_len);
        inv->global_data_len = prog->global_backup_len;
    }
}
