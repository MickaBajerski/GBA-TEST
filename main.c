#include "cpu.h"
#include <stdio.h>

// Abre a ROM especificada pelo terminal e copia os dados brutos para o array de memória virtual
int load_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Erro: Arquivo de ROM '%s' nao encontrado.\n", filename);
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Lê os dados do arquivo jogando direto no barramento do cartucho
    fread(rom_memory, 1, size, file);
    fclose(file);

    printf("ROM '%s' carregada com sucesso! (Tamanho: %.2f MB)\n", filename, (float)size / (1024*1024));
    return 0;
}

int main(int argc, char *argv[]) {
    // Validação de argumento do terminal do MinGW
    if (argc < 2) {
        printf("Uso correto no terminal:\n  %s <nome_do_jogo.gba>\n", argv[0]);
        return 1;
    }

    // Carrega o jogo escolhido
    if (load_rom(argv[1]) != 0) return 1;

    // Inicializa o motor e aponta o PC para o endereço correto do cartucho
    cpu_reset();

    printf("\n--- Inicializando Loop Principal de Execucao (Limite: 20 Ciclos) ---\n");

    int ciclos = 0;
    // O loop processará continuamente as instruções até bater em algo desconhecido ou estourar 20 voltas
    while (cpu.running && ciclos < 20) {
        printf("\n[CICLO %d] Endereco do PC Atual = 0x%08X\n", ciclos+1, cpu.r[15]);

        // 1. Busca a instrução de 32-bits via barramento
        uint32_t instr = cpu_fetch();
        printf("[FETCH] Instrucao obtida do Barramento: 0x%08X\n", instr);

        // 2 e 3. Decodifica e executa os registradores
        cpu_decode_and_execute(instr);

        ciclos++;
    }

    // Relatório final impresso no terminal após a parada do motor do emulador
    printf("\n=============================================\n");
    printf("Estado Final dos Registradores Principais:\n");
    printf("=============================================\n");
    for (int i = 0; i < 8; i++) {
        printf("R%d: 0x%08X\n", i, cpu.r[i]);
    }
    printf("PC (R15): 0x%08X\n", cpu.r[15]);
    printf("=============================================\n");
    printf("Emulador finalizado.\n");
    return 0;
}
