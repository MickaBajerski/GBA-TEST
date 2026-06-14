#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

// Definições físicas de tamanho de memória do GBA
#define ROM_MAX_SIZE  (32 * 1024 * 1024) // 32MB
#define IWRAM_SIZE    (32 * 1024)        // 32KB

// Palavras-chave 'extern' avisam ao compilador que as variáveis existem no cpu.c
extern uint8_t rom_memory[ROM_MAX_SIZE];
extern uint8_t iwram_memory[IWRAM_SIZE];

// Estrutura de dados que molda o chip ARM7TDMI
typedef struct {
    uint32_t r[16];     // Banco de registradores: R0 até R15 (R15 = PC)
    uint32_t cpsr;      // Registrador de Status Atual
    bool thumb_mode;    // Flag para alternar entre modo de 32 bits e 16 bits
    bool running;       // Controla a energia do emulador
} ARM7TDMI;

extern ARM7TDMI cpu;

// Funções de controle do processador
void cpu_reset();
uint32_t cpu_fetch();
void cpu_decode_and_execute(uint32_t instr);

// Funções do Barramento de Comunicação de Memória
uint8_t mem_read8(uint32_t addr);
uint32_t mem_read32(uint32_t addr);
void mem_write8(uint32_t addr, uint8_t value);
void mem_write32(uint32_t addr, uint32_t value); // Adicionado para suportar escritas de 4 bytes

#endif
