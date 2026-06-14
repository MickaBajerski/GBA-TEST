#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

// Memórias
uint8_t rom_memory[ROM_MAX_SIZE];
uint8_t iwram_memory[IWRAM_SIZE];

ARM7TDMI cpu;

// Barramento
uint8_t mem_read8(uint32_t addr) {
    if (addr >= 0x08000000 && addr < 0x0A000000) {
        return rom_memory[addr - 0x08000000];
    } else if (addr >= 0x03000000 && addr < 0x03008000) {
        return iwram_memory[addr - 0x03000000];
    }
    return 0;
}

uint32_t mem_read32(uint32_t addr) {
    return mem_read8(addr) |
           (mem_read8(addr+1) << 8) |
           (mem_read8(addr+2) << 16) |
           (mem_read8(addr+3) << 24);
}

void mem_write8(uint32_t addr, uint8_t value) {
    if (addr >= 0x03000000 && addr < 0x03008000) {
        iwram_memory[addr - 0x03000000] = value;
    } else if (addr >= 0x08000000 && addr < 0x0A000000) {
        printf("[BUS WARNING] Escrita ilegal na ROM: 0x%08X\n", addr);
    }
}

// CPU
void cpu_reset() {
    for (int i = 0; i < 16; i++) cpu.r[i] = 0;
    cpu.r[15] = 0x08000000;
    cpu.cpsr = 0x1F;
    cpu.thumb_mode = false;
    cpu.running = true;
    printf("CPU Resetada! PC = 0x%08X\n", cpu.r[15]);
}

uint32_t cpu_fetch() {
    uint32_t instr = mem_read32(cpu.r[15]);
    cpu.r[15] += 4;
    return instr;
}

// Decode básico
void cpu_decode_and_execute(uint32_t instr) {
    // Branch
    if ((instr & 0x0E000000) == 0x0A000000) {
        int32_t offset = (instr & 0x00FFFFFF);
        if (offset & 0x00800000) offset |= 0xFF000000;
        offset <<= 2;
        cpu.r[15] = cpu.r[15] + offset + 4;
        printf("Executando: Branch -> PC = 0x%08X\n", cpu.r[15]);
    }
    // MOV imediato
    else if ((instr & 0x0FF00000) == 0x03A00000) {
        int rd = (instr >> 12) & 0xF;
        int imm = instr & 0xFF;
        cpu.r[rd] = imm;
        printf("Executando: MOV R%d, #%d\n", rd, imm);
    }
    // ADD
    else if ((instr & 0x0FE00000) == 0x02800000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        int imm = instr & 0xFF;
        cpu.r[rd] = cpu.r[rn] + imm;
        printf("Executando: ADD R%d, R%d, #%d\n", rd, rn, imm);
    }
    // SUB
    else if ((instr & 0x0FE00000) == 0x02400000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        int imm = instr & 0xFF;
        cpu.r[rd] = cpu.r[rn] - imm;
        printf("Executando: SUB R%d, R%d, #%d\n", rd, rn, imm);
    }
    // LDR
    else if ((instr & 0x0C500000) == 0x04100000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        uint32_t addr = cpu.r[rn];
        cpu.r[rd] = mem_read32(addr);
        printf("Executando: LDR R%d, [R%d]\n", rd, rn);
    }
    // STR
    else if ((instr & 0x0C500000) == 0x04000000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        uint32_t addr = cpu.r[rn];
        mem_write8(addr, cpu.r[rd] & 0xFF);
        printf("Executando: STR R%d, [R%d]\n", rd, rn);
    }
    // BX (Branch and Exchange)
    else if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
        int rm = instr & 0xF;
        uint32_t target = cpu.r[rm];
        cpu.thumb_mode = (target & 1) ? true : false;
        cpu.r[15] = target & ~3;
        printf("Executando: BX R%d -> PC = 0x%08X | Thumb = %s\n",
               rm, cpu.r[15], cpu.thumb_mode ? "true" : "false");
    }
    else {
        printf("Instrucao nao implementada: 0x%08X\n", instr);
        cpu.running = false;
    }
}
