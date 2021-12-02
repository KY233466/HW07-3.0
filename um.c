
/**************************************************************
 *                     um.c
 * 
 *     Assignment: Homework 6 - Universal Machine Program
 *     Authors: Katie Yang (zyang11) and Pamela Melgar (pmelga01)
 *     Date: November 24, 2021
 *
 *     Purpose: This C file will hold the main driver for our Universal
 *              MachineProgram (HW6). 
 *     
 *     Success Output:
 *              
 *     Failure output:
 *              1. 
 *              2. 
 *                  
 **************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "assert.h"
#include "uarray.h"
#include "seq.h"
#include "bitpack.h"

#define BYTESIZE 8


/* Instruction retrieval */
typedef enum Um_opcode {
        CMOV = 0, SLOAD, SSTORE, ADD, MUL, DIV,
        NAND, HALT, ACTIVATE, INACTIVATE, OUT, IN, LOADP, LV
} Um_opcode;


struct Info
{
    uint32_t op;
    uint32_t rA;
    uint32_t rB;
    uint32_t rC;
    uint32_t value;
};

typedef struct Info *Info;

#define num_registers 8
#define two_pow_32 4294967296
uint32_t MIN = 0;   
uint32_t MAX = 255;
uint32_t registers_mask = 511;
uint32_t op13_mask = 268435455;

///////////////////////////////////////////////////////////////////////////////////////////
/*Memory Manager*/
struct Memory
{
    Seq_T segments;
    void* segments_lol;
    Seq_T map_queue;
};

typedef struct Memory *Memory;

///////////////////////////////////////////////////////////////////////////////////////////
/*Register Manager*/
#define num_registers 8

static inline uint32_t get_register_value(uint32_t *all_registers, uint32_t num_register);

/* Memory Manager */
Memory initialize_memory();
//uint32_t memorylength(Memory memory);         not used
uint32_t segmentlength(Memory memory, uint32_t segment_index);
void add_to_seg0(struct Memory *memory, uint32_t *word);
uint32_t get_word(Memory memory, uint32_t segment_index, 
                  uint32_t word_in_segment);
void set_word(Memory memory, uint32_t segment_index, uint32_t word_index, 
              uint32_t word);
uint32_t map_segment(Memory memory, uint32_t num_words);
void unmap_segment(Memory memory, uint32_t segment_index);
void duplicate_segment(Memory memory, uint32_t segment_to_copy);
void free_segments(Memory memory);
//////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////


/* Instruction retrieval */
Info get_Info(uint32_t instruction);

static inline void instruction_executer(Info info, Memory all_segments,
                          uint32_t *all_registers, uint32_t *counter);
//////////////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc, char *argv[])
{
    /*Progam can only run with two arguments [execute] and [machinecode_file]*/
    
    if (argc != 2) {
        fprintf(stderr, "Usage: ./um [filename] < [input] > [output]\n");
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(argv[1], "r");
    assert(fp != NULL);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* EXECUTION.C MODULE  */
    //init memory
    Memory all_segments = malloc(sizeof(struct Memory));
    all_segments->segments = Seq_new(30);
    all_segments->map_queue = Seq_new(30);
    Seq_T segment0 = Seq_new(10);

    Seq_addhi(all_segments->segments, segment0);
    
    //init registers
    uint32_t all_registers[8] = { 0 };

    /////////////////////////////////////// READ FILE //////////////////////////////
    uint32_t byte = 0; 
    uint32_t word = 0;
    int c;
    int counter = 0;

    /*Keep reading the file and parsing through "words" until EOF*/
    /*Populate the sequence every time we create a new word*/
    while ((c = fgetc(fp)) != EOF) {
        word = Bitpack_newu(word, 8, 24, c);
        counter++;
        for (int lsb = 2 * BYTESIZE; lsb >= 0; lsb = lsb - BYTESIZE) {
            byte = fgetc(fp);
            word = Bitpack_newu(word, BYTESIZE, lsb, byte);
        }
        
        uint32_t *word_ptr = malloc(sizeof(uint32_t));
        *word_ptr = word;
        
        /*Populate sequence*/
        add_to_seg0(all_segments, word_ptr);
    }

   fclose(fp);

   /////////////////////////////////////// execution //////////////////////////////

    uint32_t program_counter = 0; /*start program counter at beginning*/
    
    /*Run through all instructions in segment0, note that counter may loop*/
    while (program_counter < (segmentlength(all_segments, 0)) ) {
        uint32_t instruction = get_word(all_segments, 0, program_counter);
        Info info = get_Info(instruction);
        program_counter++;

        /*program executer is passed in to update (in loop) if needed*/
        instruction_executer(info, all_segments, all_registers, 
                             &program_counter);
    }

   // UArray_free(&(all_registers->registers));
    // for (int i = 0; i < num_registers; i++) {
    //      free((all_registers[i]));
    // }

    // free(all_registers);
    free_segments(all_segments);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    return EXIT_FAILURE; /*failure mode, out of bounds of $m[0]*/
}

uint32_t segmentlength(Memory memory, uint32_t segment_index)
{
    assert(memory != NULL);

    return Seq_length(Seq_get(memory->segments, segment_index));
}

void add_to_seg0(struct Memory *memory, uint32_t *word)
{
    assert(memory != NULL);

    Seq_T find_segment = (Seq_T)(Seq_get(memory->segments, 0));
    Seq_addhi(find_segment, word);
}

uint32_t get_word(struct Memory *memory, uint32_t segment_index, 
                  uint32_t word_in_segment)
{
    assert(memory != NULL);

    Seq_T find_segment = (Seq_T)(Seq_get(memory->segments, segment_index));
    assert(find_segment != NULL);

    uint32_t *find_word = (uint32_t *)(Seq_get(find_segment, word_in_segment));

    return *find_word;
}

void set_word(struct Memory *memory, uint32_t segment_index, 
              uint32_t word_index, uint32_t word)
{
    assert(memory != NULL);

    /*failure mode if out of bounds*/
    Seq_T find_segment = Seq_get(memory->segments, segment_index);

    assert(find_segment != NULL);/*Failure mode if refer to unmapped*/ 

    uint32_t *word_ptr = malloc(sizeof(uint32_t));
    assert(word_ptr != NULL);
    *word_ptr = word;

    /*failure mode if out of bounds*/
    uint32_t *old_word_ptr = Seq_put(find_segment, word_index, word_ptr);
    free(old_word_ptr);
}

uint32_t map_segment(struct Memory *memory, uint32_t num_words)
{
    assert(memory != NULL);
    assert(memory->segments != NULL);
    assert(memory->map_queue != NULL);

    Seq_T new_segment = Seq_new(num_words);
    /*Initialize*/
    for (uint32_t i = 0; i < num_words; i++) {
        uint32_t *word_ptr = malloc(sizeof(uint32_t));
        assert(word_ptr != NULL);

        *word_ptr = 0;
        Seq_addhi(new_segment, word_ptr);
    }

    if (Seq_length(memory->map_queue) != 0) {
        uint32_t *seq_index = (uint32_t *)Seq_remlo(memory->map_queue);
        Seq_put(memory->segments, *seq_index, new_segment);
        
        uint32_t segment_index = *seq_index;
        free(seq_index);
        return segment_index;
    }
    else {
        uint32_t length = (uint32_t)Seq_length(memory->segments);
        Seq_addhi(memory->segments, new_segment);
        return length;
    }
}

void unmap_segment(struct Memory *memory, uint32_t segment_index)
{
    assert(memory != NULL);
    assert(memory->segments != NULL);
    assert(memory->map_queue != NULL);
    /* can't un-map segment 0 */
    assert(segment_index > 0);

    Seq_T seg_to_unmap = (Seq_T)(Seq_get(memory->segments, segment_index));
    /* can't un-map a segment that isn't mapped */
    assert(seg_to_unmap != NULL);

    for (int j = 0; j < Seq_length(seg_to_unmap); j++) {
        uint32_t *word = (uint32_t *)(Seq_get(seg_to_unmap, j));
        free(word);
    }

    Seq_free(&seg_to_unmap);

    Seq_put(memory->segments, segment_index, NULL);

    uint32_t *num = malloc(sizeof(uint32_t));
    *num = segment_index;
    Seq_addhi(memory->map_queue, num);
}


void duplicate_segment(struct Memory *memory, uint32_t segment_to_copy)
{
    assert(memory != NULL);
    assert(memory->segments != NULL);
    assert(memory->map_queue != NULL);

    if (segment_to_copy != 0) {

        Seq_T seg_0 = (Seq_T)(Seq_get(memory->segments, 0));

        /*free all words in segment 0*/
        for (int j = 0; j < Seq_length(seg_0); j++) {
            uint32_t *word = (uint32_t *)(Seq_get(seg_0, j));
            free(word);
        }

        /*Free memory of old segment0*/
        Seq_free(&seg_0);

        /*hard copy - duplicate segment to create new segment 0*/
        Seq_T target = (Seq_T)Seq_get(memory->segments, segment_to_copy);
        assert(target != NULL); /*Check if copy index has been unmapped*/

        uint32_t seg_length = Seq_length(target);

        Seq_T duplicate = Seq_new(30);

        /*Willl copy every single word onto the duplicate segment*/
        for (uint32_t i = 0; i < seg_length; i++) {
            uint32_t *dup_word = malloc(sizeof(uint32_t));
            uint32_t *word = Seq_get(target, i);
            *dup_word = *word;
            
            Seq_addhi(duplicate, dup_word);
        }

        /*replace segment0 with the duplicate*/
        Seq_put(memory->segments, 0, duplicate);

    } else {
        /*don't replace segment0 with itself --- do nothing*/
        return;
    }
}


void free_segments(struct Memory *memory)
{
    assert(memory != NULL);


    uint32_t num_sequences = Seq_length(memory->segments);
    /*Free all words in each segment of memory*/
    for (uint32_t i = 0; i < num_sequences; i++)
    {
        Seq_T find_segment = (Seq_T)(Seq_get(memory->segments, i));
    
        /*Only free words in segments that are non-null*/
        if (find_segment != NULL) {
            uint32_t seg_length = Seq_length(find_segment);

            for (uint32_t j = 0; j < seg_length; j++) {
                // printf("    processing %d\n", j);
                uint32_t *word = (uint32_t *)(Seq_get(find_segment, j));
                free(word);
            }
            Seq_free(&find_segment);
        }
    }

    /* free map_queue Sequence that kept track of unmapped stuff*/
    uint32_t queue_length = Seq_length(memory->map_queue);
    
    /*free all the indexes words */
    for (uint32_t i = 0; i < queue_length; i++) {
        uint32_t *index = (uint32_t*)(Seq_get(memory->map_queue, i));
        free(index);
    }

    Seq_free(&(memory->segments));
    Seq_free(&(memory->map_queue));
    
    free(memory);
}

struct Info *get_Info(uint32_t instruction)
{
    struct Info *info = malloc(sizeof(struct Info));
    assert(info != NULL); /*Check if heap allocation successful*/

    uint32_t op = instruction;
    op = op >> 28;

    //Bitpack_getu(instruction, 4, 28);
    //lsb of 28, width of 4
    info->op = op;

    if (op != 13){
        uint32_t registers_ins = instruction &= registers_mask;

        info->rA = registers_ins >> 6;
        info->rB = registers_ins << 26 >> 29;
        info->rC = registers_ins << 29 >> 29;
    }
    else {

        uint32_t registers_ins = instruction &= op13_mask;

        info->rA = registers_ins >> 25;
        info->value = registers_ins << 7 >> 7;
    }

    return info;
}


//change back for kcachegrind
static inline void instruction_executer(Info info, Memory all_segments,
                           uint32_t *all_registers, uint32_t *counter)
{
    assert(info != NULL);
    assert(all_segments != NULL);
    assert(all_registers != NULL);
    assert(counter != NULL);

    uint32_t code = info->op;

    /*We want a halt instruction to execute quicker*/
    if (code == HALT)
    {
        /////// halt /////
        assert(info != NULL);
        assert(all_registers != NULL);
        assert(all_segments != NULL);

        free(info);
        
        free_segments(all_segments);

        exit(EXIT_SUCCESS);
    }

    /*Rest of instructions*/
    if (code == CMOV)
    {
        ///////// conditional_move /////
        assert(all_registers != NULL);

        uint32_t rA = info->rA;
        uint32_t rC = info->rC;

        uint32_t rC_val = get_register_value(all_registers, rC);

        if (rC_val != 0)
        {
            uint32_t rB = info->rB;

            uint32_t rB_val = get_register_value(all_registers, rB);

            //set_register_value(all_registers, rA, rB_val);
            uint32_t *word;
            word = &(all_registers[rA]);
            *word = rB_val;
        }
    }
    else if (code == SLOAD)
    {
        ////// segemented load /////
        /* Establishes which register indexes are being used */
        uint32_t rA = info->rA;
        uint32_t rB = info->rB;
        uint32_t rC = info->rC;

        /* Accesses the values at the register indexes*/
        uint32_t rB_val = get_register_value(all_registers, rB);
        uint32_t rC_val = get_register_value(all_registers, rC);

        uint32_t val = get_word(all_segments, rB_val, rC_val);

        //set_register_value(all_registers, rA, val);
        uint32_t *word;
        word = &(all_registers[rA]);
        *word = val;

        
    }
    else if (code == SSTORE)
    {
        /////// segmented store /////
        /* Establishes which register indexes are being used */
        uint32_t rA = info->rA;
        uint32_t rB = info->rB;
        uint32_t rC = info->rC;

        /* Accesses the values at the register indexes*/
        uint32_t rA_val = get_register_value(all_registers, rA);
        uint32_t rB_val = get_register_value(all_registers, rB);
        uint32_t rC_val = get_register_value(all_registers, rC);

        set_word(all_segments, rA_val, rB_val, rC_val);
    }
    else if (code == ADD || code == MUL || code == DIV || code == NAND)
    {
        ////// arithetics /////
        uint32_t rA = info->rA;
        uint32_t rB = info->rB;
        uint32_t rC = info->rC;

        uint32_t rB_val = get_register_value(all_registers, rB);
        uint32_t rC_val = get_register_value(all_registers, rC);

        uint32_t value = 0;

        /*Determine which math operation to perform based on 4bit opcode*/
        if (code == ADD)
        {
            value = (rB_val + rC_val) % two_pow_32;
        }
        else if (code == MUL)
        {
            value = (rB_val * rC_val) % two_pow_32;
        }
        else if (code == DIV)
        {
            value = rB_val / rC_val;
        }
        else if (code == NAND)
        {
            value = ~(rB_val & rC_val);
        }
        else
        {
            exit(EXIT_FAILURE);
        }

        //set_register_value(all_registers, rA, value);
        uint32_t *word;
        word = &(all_registers[rA]);
        *word = value;
    }
    else if (code == ACTIVATE)
    {
        ////////////// map_a_segment ////////
        assert(all_registers != NULL);
        assert(all_segments != NULL);

        uint32_t rB = info->rB;
        uint32_t rC_val = get_register_value(all_registers, info->rC);
        uint32_t mapped_index = map_segment(all_segments, rC_val);

       // set_register_value(all_registers, rB, mapped_index);
        uint32_t *word;
        word = &(all_registers[rB]);
        *word = mapped_index;
    }
    else if (code == INACTIVATE)
    {
        ////////////// unmap_a_segment ////////
        assert(all_registers != NULL);
        assert(all_segments != NULL);

        uint32_t rC_val = get_register_value(all_registers, info->rC);
        unmap_segment(all_segments, rC_val);
    }
    else if (code == OUT)
    {
        ////////////// output ////////
        assert(all_registers != NULL);

        uint32_t rC = info->rC;
        uint32_t val = get_register_value(all_registers, rC);

        assert(val >= MIN);
        assert(val <= MAX);

        printf("%c", val);
    }
    else if (code == IN)
    {
        ////////////// input ////////
        assert(all_registers != NULL);

        uint32_t rC = info->rC;

        uint32_t input_value = (uint32_t)fgetc(stdin);
        uint32_t all_ones = ~0;

        /*Check if input value is EOF...aka: -1*/
        if (input_value == all_ones)
        {
            //set_register_value(all_registers, rC, all_ones);
            uint32_t *word;
            word = &(all_registers[rC]);
            *word = all_ones;
            
            return;
        }

        /* Check if the input value is in bounds */
        assert(input_value >= MIN);
        assert(input_value <= MAX);

        /* $r[C] gets loaded with input value */

        uint32_t *word;
        word = &(all_registers[rC]);
        *word = input_value;
    }
    else if (code == LOADP)
    {
        ////////////// load_program ////////
        assert(all_registers != NULL);
        assert(all_segments != NULL);
        assert(counter != NULL);

        //uint32_t rB_val = get_register_value(all_registers, info->rB);
        uint32_t rB_val = all_registers[info->rB];

        uint32_t rC_val = get_register_value(all_registers, info->rC);
        duplicate_segment(all_segments, rB_val);
        *counter = rC_val;
    }
    else if (code == LV)
    {
        ////////////// load_value ////////
        assert(all_registers != NULL);

        uint32_t rA = info->rA;
        uint32_t val = info->value;
       // set_register_value(all_registers, rA, val);

        uint32_t *word= &(all_registers[rA]);
        *word = val;
    }
    else
    {
        exit(EXIT_FAILURE);
    }

    free(info);
    return;
}

static inline uint32_t get_register_value(uint32_t *all_registers, uint32_t num_register)
{
    return all_registers[num_register]; /////????
}
