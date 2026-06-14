#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

// Alocação real das memórias físicas representadas no computador
uint8_t rom_memory[ROM_MAX_SIZE];
uint8_t iwram_memory[IWRAM_SIZE];

ARM7TDMI cpu;

// ============================================================================
// LOGICA DO BARRAMENTO DE MEMÓRIA (BUS)
// ============================================================================
uint8_t mem_read8(uint32_t addr) {
    if (addr >= 0x08000000 && addr < 0x0A000000) {
        return rom_memory[addr - 0x08000000];
    } else if (addr >= 0x03000000 && addr < 0x03008000) {
        return iwram_memory[addr - 0x03000000];
    }
    return 0;
}

uint32_t mem_read32(uint32_t addr) {
    addr &= ~3; // Alinhamento obrigatório de 4 bytes na arquitetura ARM
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

// CORREÇÃO: Escreve uma palavra inteira de 32 bits (4 bytes) na RAM de forma alinhada
void mem_write32(uint32_t addr, uint32_t value) {
    addr &= ~3;
    mem_write8(addr,     value & 0xFF);
    mem_write8(addr + 1, (value >> 8) & 0xFF);
    mem_write8(addr + 2, (value >> 16) & 0xFF);
    mem_write8(addr + 3, (value >> 24) & 0xFF);
}

// ============================================================================
// CONTROLE DO PROCESSADOR (CPU)
// ============================================================================
void cpu_reset() {
    for (int i = 0; i < 16; i++) cpu.r[i] = 0;
    cpu.r[15] = 0x08000000; // Ponto de partida padrão dos cartuchos de GBA
    cpu.cpsr = 0x1F;        // Inicializa em Modo de Sistema
    cpu.thumb_mode = false;
    cpu.running = true;
    printf("CPU Resetada! PC = 0x%08X\n", cpu.r[15]);
}

uint32_t cpu_fetch() {
    uint32_t instr = mem_read32(cpu.r[15]);
    cpu.r[15] += 4; // Avança o PC em 4 bytes (Tamanho de uma instrução ARM)
    return instr;
}

// ============================================================================
// CIRCUITO DE DECODIFICAÇÃO E EXECUÇÃO (DECODE & EXECUTE)
// ============================================================================
void cpu_decode_and_execute(uint32_t instr) {

    // 1. Instrução: Branch (Pulo)
    if ((instr & 0x0E000000) == 0x0A000000) {
        int32_t offset = (instr & 0x00FFFFFF);
        if (offset & 0x00800000) offset |= 0xFF000000; // Extensão de sinal para negativos
        offset <<= 2; // Multiplica o salto por 4
        cpu.r[15] = cpu.r[15] + offset + 4; // Aplica o salto ajustando o atraso de Pipeline
        printf("Executando: Branch -> PC = 0x%08X\n", cpu.r[15]);
    }
    // 2. Instrução: MOV Imediato (Formato direto por deslocamento de bits)
    // Deslocamos 21 bits para a direita para alinhar o Opcode e verificamos se ele vale 13 (0x0D)
    else if (((instr >> 21) & 0xF) == 0x0D) {
        int rd = (instr >> 12) & 0xF;
        int imm = instr & 0xFF;
        cpu.r[rd] = imm;
        printf("Executando: MOV R%d, #%d\n", rd, imm);
    }


    // 3. Instrução: ADD Imediato
    else if ((instr & 0x0FE00000) == 0x02800000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        int imm = instr & 0xFF;
        cpu.r[rd] = cpu.r[rn] + imm;
        printf("Executando: ADD R%d, R%d, #%d\n", rd, rn, imm);
    }

    // 4. Instrução: SUB Imediato
    else if ((instr & 0x0FE00000) == 0x02400000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        int imm = instr & 0xFF;
        cpu.r[rd] = cpu.r[rn] - imm;
        printf("Executando: SUB R%d, R%d, #%d\n", rd, rn, imm);
    }

    // 5. Instrução: LDR (Load Register) - CORREÇÃO: Suporta o cálculo de Offset em 12 bits
    else if ((instr & 0x0C500000) == 0x04100000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        int offset = instr & 0xFFF; // Isola os 12 bits inferiores do deslocamento

        uint32_t addr = cpu.r[rn];
        // Bit 23 (U) define a direção do offset: 1 = Soma, 0 = Subtrai
        if (instr & (1 << 23)) addr += offset;
        else addr -= offset;

        cpu.r[rd] = mem_read32(addr);
        printf("Executando: LDR R%d, [R%d, #%d] -> Endereco lido: 0x%08X\n", rd, rn, offset, addr);
    }

    // 6. Instrução: STR (Store Register) - CORREÇÃO: Gravando palavra completa de 32-bits com Offset
    else if ((instr & 0x0C500000) == 0x04000000) {
        int rd = (instr >> 12) & 0xF;
        int rn = (instr >> 16) & 0xF;
        int offset = instr & 0xFFF;

        uint32_t addr = cpu.r[rn];
        if (instr & (1 << 23)) addr += offset;
        else addr -= offset;

        mem_write32(addr, cpu.r[rd]); // Gravando 4 bytes inteiros na RAM
        printf("Executando: STR R%d, [R%d, #%d] -> Endereco gravado: 0x%08X\n", rd, rn, offset, addr);
    }

    // Fallback de segurança para encerrar se bater em um byte que o core ainda não sabe o que é
    else {
        printf("Instrucao nao implementada: 0x%08X\n", instr);
        cpu.running = false;
    }
}
