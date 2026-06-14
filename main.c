#include <stdio.h>      // Entrada e saída padrão (printf)
#include <stdlib.h>     // Gerenciamento de memória e arquivos
#include <stdint.h>     // Tipos de dados de tamanho fixo (uint32_t, uint8_t)
#include <stdbool.h>    // Tipo booleano (true/false)

// O GBA suporta até 32MB de dados mapeados para o cartucho (ROM)
#define ROM_MAX_SIZE (32 * 1024 * 1024)

// Memória RAM virtual para armazenar os bytes da nossa ROM
uint8_t rom_memory[ROM_MAX_SIZE];

// ============================================================================
// ESTRUTURA DE DADOS DA CPU ARM7TDMI
// ============================================================================
typedef struct {
    uint32_t r[16];     // Registradores gerais R0 a R15 (R15 = PC)
    uint32_t cpsr;      // Registrador de status atual (flags do processador)
    bool thumb_mode;    // Controla se estamos em modo ARM (32-bit) or Thumb (16-bit)
    bool running;       // Flag para o loop do emulador saber se a CPU esta ligada
} ARM7TDMI;

// Instância global da nossa CPU virtual
ARM7TDMI cpu;

// ============================================================================
// FUNÇÃO PARA RESETAR / INICIALIZAR A CPU
// ============================================================================
void cpu_reset() {
    for (int i = 0; i < 16; i++) {
        cpu.r[i] = 0;
    }

    // Endereço de memória física padrão onde o hardware do GBA enxerga a ROM
    cpu.r[15] = 0x08000000;

    // Coloca a CPU em Modo de Sistema com interrupções desativadas
    cpu.cpsr = 0x0000001F;
    cpu.thumb_mode = false; // Começa em modo ARM (32-bit)
    cpu.running = true;     // Liga o motor da CPU

    printf("CPU Resetada! PC inicializado em: 0x%08X\n", cpu.r[15]);
}

// ============================================================================
// FUNÇÃO PARA CARREGAR A ROM
// ============================================================================
int load_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Erro Critico: Nao foi possivel abrir a ROM '%s'\n", filename);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Arquivo '%s' carregado com sucesso!\n", filename);
    printf("Tamanho: %ld bytes (%.2f MB)\n", file_size, (float)file_size / (1024 * 1024));

    if (file_size > ROM_MAX_SIZE) {
        printf("Erro Critico: ROM maior que o limite de 32MB.\n");
        fclose(file);
        return -1;
    }

    size_t bytes_read = fread(rom_memory, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        printf("Erro Critico: Falha na leitura dos bytes.\n");
        return -1;
    }

    return 0;
}

// ============================================================================
// PASSO 1: BUSCA (FETCH)
// ============================================================================
uint32_t cpu_fetch() {
    // Subtrai o endereço base (0x08000000) para achar a posição real no nosso array
    uint32_t real_address = cpu.r[15] - 0x08000000;
    uint32_t instruction = 0;

    if (!cpu.thumb_mode) {
        // Monta a palavra de 32 bits no formato Little-Endian
        instruction = rom_memory[real_address] |
                      (rom_memory[real_address + 1] << 8) |
                      (rom_memory[real_address + 2] << 16) |
                      (rom_memory[real_address + 3] << 24);

        // Em modo ARM, avançamos o PC em 4 bytes para apontar para a próxima instrução
        cpu.r[15] += 4;
    } else {
        // Modo Thumb (16 bits)
        instruction = rom_memory[real_address] | (rom_memory[real_address + 1] << 8);
        cpu.r[15] += 2;
    }

    return instruction;
}

// ============================================================================
// PASSO 2 E 3: DECODIFICACAO (DECODE) E EXECUCAO (EXECUTE)
// ============================================================================
void cpu_decode_and_execute(uint32_t instruction) {

    // ------------------------------------------------------------------------
    // IDENTIFICACAO 1: Instrucao de Pulo (Branch / B)
    // Bits 25 a 27 do ARM precisam ser 0b101 (Máscara 0x0E000000 == 0x0A000000)
    // ------------------------------------------------------------------------
    if ((instruction & 0x0E000000) == 0x0A000000) {
        // Encontramos uma instrucao Branch (B)!
        int32_t offset = (instruction & 0x00FFFFFF);

        // Extensão de sinal para números negativos (se o bit 23 for 1)
        if (offset & 0x00800000) {
            offset |= 0xFF000000;
        }

        // Multiplica por 4 deslocando dois bits para a esquerda
        offset <<= 2;

        printf("[DECODE] Instrucao: B (Branch/Pulo)\n");
        printf("[DECODE] Distancia do pulo: %d bytes\n", offset);

        // Aplica o pulo corrigindo o Pipeline (+4 extra adicionado ao PC atual)
        cpu.r[15] = cpu.r[15] + offset + 4;

        printf("[EXECUTE] Pulo executado! Novo PC: 0x%08X\n", cpu.r[15]);
    }

    // ------------------------------------------------------------------------
    // IDENTIFICACAO 2: Instrucao de Mover Valor (MOV com Valor Imediato)
    // Bits 25 a 27 mudam para indicar processamento de dados com imediato (0b001)
    // Bits 21 a 24 definem o Opcode do processamento. Para o MOV, o Opcode eh 0b1101 (13 em decimal)
    // Combinando esses bits na mascara 0x0DE00000, procuramos o padrao 0x03A00000
    // ------------------------------------------------------------------------
    else if ((instruction & 0x0DE00000) == 0x03A00000) {
        // Encontramos uma instrucao MOV!

        // Bits 12 a 15 definem o "Registrador de Destino" (Rd) -> onde vamos salvar o valor
        uint8_t rd = (instruction >> 12) & 0x0F;

        // Bits 0 a 7 guardam o "Valor Imediato" -> o numero puro (0 a 255) que queremos guardar
        uint32_t immediate_value = instruction & 0x000000FF;

        printf("[DECODE] Instrucao: MOV (Mover Valor Imediato)\n");
        printf("[DECODE] Destino: Registrador R%d\n", rd);
        printf("[DECODE] Valor a ser movido: %d (0x%02X)\n", immediate_value, immediate_value);

        // Executa a acao gravando o valor direto no registrador escolhido da CPU
        cpu.r[rd] = immediate_value;

        printf("[EXECUTE] R%d agora vale: 0x%08X\n", rd, cpu.r[rd]);
    }

    // ------------------------------------------------------------------------
    // SE CAIR AQUI: Instrucao ainda nao mapeada no emulador
    // ------------------------------------------------------------------------
    else {
        printf("[DECODE] Instrucao desconhecida ou nao implementada: 0x%08X\n", instruction);
        cpu.running = false; // Desliga o loop para seguranca
    }
}

// ============================================================================
// PONTO DE ENTRADA PRINCIPAL (MAIN)
// ============================================================================
int main(int argc, char *argv[]) {
    printf("=============================================\n");
    printf("        Emulador GBA - Modo Monolitico       \n");
    printf("=============================================\n");

    if (argc < 2) {
        printf("Erro: Informe a ROM. Exemplo: %s Sonic.gba\n", argv[0]);
        return 1;
    }

    // AQUI ESTÁ A CORREÇÃO: Passando argv[1] em vez de argv
    if (load_rom(argv[1]) != 0) {
        return 1;
    }

    // Inicializa os registradores
    cpu_reset();

    printf("\n--- Iniciando o Loop de Execucao Principal ---\n");

    int ciclos = 0;
    while (cpu.running && ciclos < 10) {
        printf("\n[CICLO %d] PC Atual: 0x%08X\n", ciclos + 1, cpu.r[15]);

        // 1. Busca
        uint32_t opcode = cpu_fetch();
        printf("[FETCH] Instrucao obtida: 0x%08X\n", opcode);

        // 2 e 3. Decodifica e Executa
        cpu_decode_and_execute(opcode);

        ciclos++;
    }

    printf("\n=============================================\n");
    printf("Estado Final dos Registradores Principais:\n");
    for(int i = 0; i < 8; i++) {
        printf("R%d: 0x%08X   ", i, cpu.r[i]);
        if(i == 3) printf("\n");
    }
    printf("\nPC (R15): 0x%08X\n", cpu.r[15]);
    printf("=============================================\n");
    printf("Emulador finalizado.\n");
    return 0;
}
