#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <Windows.h>
#include <conio.h>  // _kbhit

/* Compile with:  g++lc3.c -o lc3-vm.exe*/
/* Run with:      .\lc3-vm.exe programs\2048.obj*/

#define MEMORY_MAX (1 << 16)

#define DESTINATION_REGISTER(i) (((i) >> 9) & 0x7)
#define BASE_REGISTER(i) (((i) >> 6) & 0x7)
#define FIRST_OPERAND(i) (((i) >> 6) & 0x7)
#define IMMEDIATE_FLAG(i) (((i) >> 5) & 0x1)
#define CONDITIONAL_FLAG(i) (((i) >> 9) & 0x7)
#define PC_OFFSET(i) sign_extend((i) & 0x1FF, 9)
#define SWAP_16(x) (((x) << 8) | ((x) >> 8))

int running = 0;

enum
{
    R_R0 = 0, /* General Registers */
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* Program Counter */
    R_COND, /* Condition Flag */
    R_COUNT /* Register Count */
};

enum
{
    MR_KBSR = 0xFE00, /* Keyboard Status */
    MR_KBDR = 0xFE02  /* Keyboard Data */
};

enum
{
    OP_BR = 0, /* Branch */
    OP_ADD,    /* Add  */
    OP_LD,     /* Load */
    OP_ST,     /* Store */
    OP_JSR,    /* Jump Register */
    OP_AND,    /* Bitwise And */
    OP_LDR,    /* Load Register */
    OP_STR,    /* Store Register */
    OP_RTI,    /* Unused */
    OP_NOT,    /* Bitwise Not */
    OP_LDI,    /* Load Indirect */
    OP_STI,    /* Store Indirect */
    OP_JMP,    /* Jump */
    OP_RES,    /* Unused */
    OP_LEA,    /* Load Effective Address */
    OP_TRAP    /* Execute Trap */
};

enum
{
    TRAP_GETC = 0x20,  /* Get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* Output a character */
    TRAP_PUTS = 0x22,  /* Output a word string */
    TRAP_IN = 0x23,    /* Get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* Output a byte string */
    TRAP_HALT = 0x25   /* Halt the program */
};

enum
{
    FL_POS = 1 << 0, /* Pos */
    FL_ZRO = 1 << 1, /* Zero */
    FL_NEG = 1 << 2, /* Neg */
};

uint16_t reg[R_COUNT];
uint16_t memory[MEMORY_MAX];

/* Input Buffering */
HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); /* save old mode */
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or
                                    more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering() {
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key() {
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

void handle_interrupt(int signal) {
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

void read_image_file(FILE* file){
    uint16_t origin; /* Where to place Image */
    fread(&origin, sizeof(origin), 1, file);
    origin = SWAP_16(origin);

    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* Swap to Little Endian */
    while (read-- > 0) {
        *p = SWAP_16(*p);
        ++p;
    }
}

int read_image(const char* image_path) {
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}

void mem_write(uint16_t address, uint16_t val) {
    memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
    if (address == MR_KBSR) {
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_flags(uint16_t r) {
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) { /* A 1 in the left-most bit indicates negative */
        reg[R_COND] = FL_NEG;
    }
    else {
        reg[R_COND] = FL_POS;
    }
}

void trap_puts() {
    uint16_t* c = memory + reg[R_R0];
    while (*c) {
        putc((char)*c, stdout);
        ++c;
    }
    fflush(stdout);
}

void trap_input() {
    reg[R_R0] = (uint16_t) getchar();
    update_flags(R_R0);
}

void trap_output() {
    putc((char) reg[R_R0], stdout);
    fflush(stdout);
}

void trap_output_string() {
    uint16_t* c = memory + reg[R_R0];
    while (*c) {
        char char1 = (*c) & 0xFF;
        putc(char1, stdout);

        char char2 = (*c) >> 8;
        if (char2) putc(char2, stdout);
        ++c;
    }
    fflush(stdout);
}

void trap_input_prompt() {
    printf("Enter a character: ");

    char c = getchar();
    putc(c, stdout);
    fflush(stdout);

    reg[R_R0] = (uint16_t) c;
    update_flags(R_R0);
}

void trap_halt() {
    puts("HALT");
    fflush(stdout);
    running = 0;
}

void trap_switch(uint16_t i) {
    reg[R_R7] = reg[R_PC];
    switch (i & 0xFF) {
        case TRAP_GETC:
            trap_input();
            break;
        case TRAP_OUT:
            trap_output();
            break;
        case TRAP_PUTS:
            trap_puts();
            break;
        case TRAP_IN:
            trap_input_prompt();
            break;
        case TRAP_PUTSP:
            trap_output_string();
            break;
        case TRAP_HALT:
            trap_halt();
            break;
    }
}

void add(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    uint16_t r1 = FIRST_OPERAND(i);

    if (IMMEDIATE_FLAG(i)) {
        uint16_t imm5 = sign_extend(i & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    }
    else {
        uint16_t r2 = i & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    update_flags(r0);
}

void bitwise_and(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    uint16_t r1 = FIRST_OPERAND(i);

    if (IMMEDIATE_FLAG(i)) {
        uint16_t imm5 = sign_extend(i & 0x1F, 5);
        reg[r0] = reg[r1] & imm5;
    }
    else {
        uint16_t r2 = i & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }
    update_flags(r0);
}

void bitwise_not(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    uint16_t r1 = FIRST_OPERAND(i);

    reg[r0] = ~reg[r1];
    update_flags(r0);

}

void branch(uint16_t i) {
    if (CONDITIONAL_FLAG(i) & reg[R_COND]){
        reg[R_PC] += PC_OFFSET(i);
    }
}

void jump(uint16_t i) {
    reg[R_PC] = reg[BASE_REGISTER(i)];
}

void jump_register(uint16_t i) {
    uint16_t long_flag = (i >> 11) & 1;
    reg[R_R7] = reg[R_PC];

    if (long_flag) {
        uint16_t long_pc_offset = sign_extend(i & 0x7FF, 11);
        reg[R_PC] += long_pc_offset;  /* JSR */
    }

    else {
        jump(i); /* JSRR */
    }
}

void load(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    reg[r0] = mem_read(reg[R_PC] + PC_OFFSET(i));
    update_flags(r0);
}

void load_indirect(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    reg[r0] = mem_read(mem_read(reg[R_PC] + PC_OFFSET(i)));
    update_flags(r0);
}

void load_register(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    uint16_t offset = sign_extend(i & 0x3F, 6);
    reg[r0] = mem_read(reg[BASE_REGISTER(i)] + offset);
    update_flags(r0);
}

void load_effective_address(uint16_t i) {
    uint16_t r0 = DESTINATION_REGISTER(i);
    reg[r0] = reg[R_PC] + PC_OFFSET(i);
    update_flags(r0);
}

void store(uint16_t i) {
    mem_write(reg[R_PC] + PC_OFFSET(i), reg[DESTINATION_REGISTER(i)]);
}

void store_indirect(uint16_t i) {
    mem_write(mem_read(reg[R_PC] + PC_OFFSET(i)), reg[DESTINATION_REGISTER(i)]);
}

void store_register(uint16_t i){
    uint16_t offset = sign_extend(i & 0x3F, 6);
    mem_write(reg[BASE_REGISTER(i)] + offset, reg[DESTINATION_REGISTER(i)]);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    if (argc < 2) {
        printf("lc3 [image-file1] ...\n"); /* Usage String */
        exit(2);
    }

    for (int j = 1; j < argc; ++j) {
        if (!read_image(argv[j])) {
            printf("Failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    reg[R_COND] = FL_ZRO;
    reg[R_PC] = 0x3000; /* 0x3000 is default PC*/

    running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12; /* Get first 4 bits (Op bits) */

        switch (op)
        {
            case OP_ADD:
                add(instr);
                break;
            case OP_AND:
                bitwise_and(instr);
                break;
            case OP_NOT:
                bitwise_not(instr);
                break;
            case OP_BR:
                branch(instr);
                break;
            case OP_JMP:
                jump(instr);
                break;
            case OP_JSR:
                jump_register(instr);
                break;
            case OP_LD:
                load(instr);
                break;
            case OP_LDI:
                load_indirect(instr);
                break;
            case OP_LDR:
                load_register(instr);
                break;
            case OP_LEA:
                load_effective_address(instr);
                break;
            case OP_ST:
                store(instr);
                break;
            case OP_STI:
                store_indirect(instr);
                break;
            case OP_STR:
                store_register(instr);
                break;
            case OP_TRAP:
                trap_switch(instr);
                break;
            case OP_RES:
                abort();
            case OP_RTI:
                abort();
            default:
                break;
        }
    }

    restore_input_buffering();
    return 0;
}