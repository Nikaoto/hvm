/*
 hvm - hack virtual machine

 Usage: hvm infile [-o outfile]

 Generates hack assembly code using vm instructions from `infile` and writes it
 out to `infile.asm`.

 Options:
     -o outfile      specify output file
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "file.h"

#define MIN_ARGC                           2
#define MAX_ARGC                           4
#define ERR_TEXT_SIZE                      200
#define FILE_PATH_SIZE                     200

#define INST_ARRAY_INITIAL_CAPACITY        1024
#define INST_ARRAY_CAPACITY_GROWTH_RATE    1024
#define COMMENT_HEADERS_IN_OUTFILE         1
#define INITIAL_HACK_CODE_SIZE_PER_INST    8

// str.c stuff
#define CHAR_ARR_GROWTH_RATE               256

void char_arr_free(char *str)
{
    free(((int*)(void*)str - 2));
}

char *char_arr_append(char *str, char *app)
{
    int app_len = strlen(app);;
    int *p = NULL;

    // Grow if necessary
    if (str == 0 || app_len + *((int*)(void*)str - 1) >= *((int*)(void*)str - 2)) {
        int grow_amount = CHAR_ARR_GROWTH_RATE > app_len ? CHAR_ARR_GROWTH_RATE : app_len;
        int curr_size = str ? *((int*)(void*)str - 2) : 0;
        int new_size = sizeof(char) * (curr_size + grow_amount) + sizeof(int)*2;
        p = (int*) realloc(str ? *((int*)(void*)str - 2) : 0, new_size);
    }

    // Append new chars
    for (int i = 0; i < app_len; i++)
        p[2+i] = app_len;

    return p+2;
}

// end str.c stuff

enum ACTION {
    ACTION_POP = 0,
    ACTION_PUSH,
    ACTION_ADD,
    ACTION_SUB,
    ACTION_NEG,
    ACTION_EQ,
    ACTION_GT,
    ACTION_LT,
    ACTION_AND,
    ACTION_OR,
    ACTION_NOT,
    ACTION_PARSE_ERROR,
};

enum SEGMENT {
    SEGMENT_NONE = 0,
    SEGMENT_ARGUMENT,
    SEGMENT_LOCAL,
    SEGMENT_STATIC,
    SEGMENT_CONSTANT,
    SEGMENT_THIS,
    SEGMENT_THAT,
    SEGMENT_POINTER,
    SEGMENT_TEMP,
    SEGMENT_PARSE_ERROR,
};

char *INST_ACTION_STRINGS[] = {
    [ACTION_POP]  = "pop",
    [ACTION_PUSH] = "push",
    [ACTION_ADD]  = "add",
    [ACTION_SUB]  = "sub",
    [ACTION_NEG]  = "neg",
    [ACTION_EQ]   = "eq",
    [ACTION_GT]   = "gt",
    [ACTION_LT]   = "lt",
    [ACTION_AND]  = "and",
    [ACTION_OR]   = "or",
    [ACTION_NOT]  = "not",
};

char *INST_SEGMENT_STRINGS[] = {
    [SEGMENT_NONE]     = "NONE",
    [SEGMENT_ARGUMENT] = "argument",
    [SEGMENT_LOCAL]    = "local",
    [SEGMENT_STATIC]   = "static",
    [SEGMENT_CONSTANT] = "constant",
    [SEGMENT_THIS]     = "this",
    [SEGMENT_THAT]     = "that",
    [SEGMENT_POINTER]  = "pointer",
    [SEGMENT_TEMP]     = "temp",
};

typedef struct {
    enum ACTION action;
    enum SEGMENT segment;
    int number;
} Instruction;

void print_instruction(Instruction *inst)
{
    printf("action=%s, segment=%s, number=%i\n",
        INST_ACTION_STRINGS[inst->action],
        INST_SEGMENT_STRINGS[inst->segment],
        inst->number);
}

int snprintf_instruction(char *str, size_t size, Instruction *inst)
{
    if (inst->segment == SEGMENT_NONE)
        return snprintf(str, size, "%s", INST_ACTION_STRINGS[inst->action]);
    return snprintf(str, size, "%s %s %i",
        INST_ACTION_STRINGS[inst->action],
        INST_SEGMENT_STRINGS[inst->segment],
        inst->number);
}

// String, but defined by a range in memory (inclusive)
// Doesn't have to be null-terminated
typedef struct {
    char *start; // inclusive
    char *end; // inclusive
} Slice;

inline int is_number(char c)
{
    return c >= '0' && c <= '9';
}

// Returns a^b
// 'b' must be non-negative (given the return type of the function)
int power(int a, int b)
{
    if (a == 0)
        return 0;

    int ans = 1;
    while (b-- > 0)
        ans *= a;

    return ans;
}

// Parses and returns first int from 'buf' to 'end' inclusive.
// NOTE: The given range MUST ONLY contain digits or a leading minus,
//  otherwise, undefined behavior
int parse_next_int(char *buf, char *end)
{
    int num = 0;
    int negative = 0;

    // Check if negative
    if (*buf == '-') {
        negative = 1;
        buf++;
    }

    // Skip leading zeros
    if (*buf == '0') {
        while (*(buf + 1) == '0') {
            buf++;
        }
    }

    // Build num up backwards
    char *p = end;
    size_t rpos = 0; // Position of current digit from right
    while (p >= buf) {
        int n = *p - '0';
        num += n * power(10, rpos);
        p--;
        rpos++;
    }

    if (negative)
        return -num;

    return num;
}

// Return pointer to first occurrence of 'pat' in 'str'
// Checks from 'str' to 'end' inclusive
// Return NULL if not found
char *strstr_range(char *str, char *end, char *pat)
{
    char *pat_start = pat;
    while (*str && (str <= end)) {
        while (*str++ == *pat++) {
            if (*pat == '\0') {
                // Pattern end reached
                size_t pat_len = pat - pat_start;
                return str - pat_len;
            }
        }
        pat = pat_start; // Reset pat
    }

    return NULL;
}

// Find next occurence of any char from 'chars' or '\0' in 'str' and
// return pointer to it.
// Both 'chars' and 'str' must be null-terminated, so it doesn't halt
char *find_next_any(char *str, char *chars)
{
    char *chars_start = chars;
    for (; *str != '\0'; str++) {
        for (; *chars != '\0'; chars++) {
            if (*str == *chars)
                return str;
        }
        chars = chars_start;
    }

    return str;
}

// Find next index of any char from 'c' or '\0' after 'str[i]'
// Return last index (length - 1) if not found
// Both 'c' and 'str' must be null-terminated, so it doesn't halt
inline size_t find_next_any_index(char *str, size_t i, char *c)
{
    return find_next_any(str + i, c) - str;
}

// Return 1 if str begins with pat (only stopping when pat reaches '\0')
// Return 0 otherwise, or if str ends before pat
int str_begins_with(char *str, char *pat)
{
    while (*pat != '\0') {
        if (*str != *pat)
            return 0;
        pat++;
        if (*pat == '\0')
            return 1;
        str++;
    }
    return 1;
}

// Return index of first occurrence of t in s
// Return -1 if t not found in s
int strindex(char *s, char *t)
{
    char *s_start = s;
    char *t_start = t;
    while (*s) {
        while (*s++ == *t++) {
            if (*t == 0) // Return index
                return s - s_start - (t - t_start);
        }
        t = t_start; // Reset t
    }
    return -1;
}

// Return index of last occurrence of t in s
// Return -1 if t not found in s
int strindex_last(char *s, char *t)
{
    size_t s_len = strlen(s);
    size_t t_len = strlen(t);

    char *s_start = s;
    char *s_end = s + s_len - 1;
    s = s_end;
    char *t_start = t;
    char *t_end = t + t_len - 1;
    t = t_end;

    while (s != s_start) {
        while (*s-- == *t--) {
            if (t == t_start && *s == *t) // Return index
                return s - s_start;
        }
        t = t_end; // Reset t
    }
    return -1;
}

// Replaces last occurrence of sub in str with rep
// Returns 0 on success
// Returns 1 on failure
// str must be large enough to fit rep
int str_replace_last(char *str, char *sub, char *rep)
{
    int index = strindex_last(str, sub);
    if (index == -1) {
        return 1;
    }

    while (*rep)
        str[index++] = *rep++;

    return 0;
}

// Sets input_file, output_file, and error_text
// Returns 0 on success, 1 on error
int parse_arguments(int argc, char* argv[],
    char *input_file, char *output_file, char *error_text)
{
    if (argc > MAX_ARGC) {
        strcpy(error_text, "error: too many arguments");
        return 1;
    }

    if (argc < MIN_ARGC) {
        strcpy(error_text, "error: input file not given");
        return 1;
    }

    *input_file = 0;
    *output_file = 0;

    for (int i = 1; i < argc; i++) {
        // Handle -o switch
        if (strindex(argv[i], "-o") > -1) {
            // Exit if multiple output files given
            if (*output_file != 0) {
                strcpy(error_text, "error: too many output files");
                return 1;
            }

            // Exit if option not spaced (ex: -ofile)
            // TODO better to separate -ofile into "-o" and "file" before
            //  passing argv to this function
            if (argv[i][2] != 0) {
                snprintf(error_text, ERR_TEXT_SIZE,
                    "error: unrecognized argument '%s'", argv[i]);
                return 1;
            }

            // Grab next arg as output_file
            if (i + 1 >= argc) {
                strcpy(error_text, "error: expected output file after '-o'");
                return 1;
            }
            strncpy(output_file, argv[i + 1], FILE_PATH_SIZE);
            i++;
        } else { // Argument is input_file
            // Exit if multiple input files given
            if (*input_file != 0) {
                strcpy(error_text, "error: too many input files");
                return 1;
            }
            strncpy(input_file, argv[i], FILE_PATH_SIZE);
        }
    }

    if (*input_file == 0) {
        strcpy(error_text, "error: input file not given");
        return 1;
    }

    if (*output_file == 0) {
        strncpy(output_file, input_file, FILE_PATH_SIZE);
        int err = str_replace_last(output_file, ".vm", ".asm");
        if (err != 0) { // input_file doesn't end with .asm
            strncat(output_file, ".asm",
                FILE_PATH_SIZE - strlen(output_file));
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    char input_file_path[FILE_PATH_SIZE];
    char output_file_path[FILE_PATH_SIZE];
    char error_text[ERR_TEXT_SIZE];

    // Parse arguments
    int err = parse_arguments(argc, argv, input_file_path, output_file_path,
        error_text);
    if (err == 1) {
        printf("%s\n", error_text);
        return 1;
    }

    size_t instructions_capacity = INST_ARRAY_INITIAL_CAPACITY;
    Instruction *instructions = malloc(sizeof(Instruction) *
        instructions_capacity);

    // Completely read file into a buffer
    size_t input_file_size;
    char *input_buf = load_file(input_file_path, &input_file_size);

    // Parse all instructions into instructions array
    size_t inst_count = 0;
    size_t src_line_count = 1; // for pointing out errors
    for (size_t i = 0; input_buf[i] != '\0';) {
        // Skip whitespace
        switch (input_buf[i]) {
        case ' ':
        case '\t':
        case '\r':
            i++;
            continue;
        case '\n':
            i++;
            src_line_count++;
            continue;
        }

        // Handle line starting with comment
        if (input_buf[i] == '/' && input_buf[i+1] == '/') {
            // Skip rest of line
            i = find_next_any_index(input_buf, i, "\r\n") + 1;
            continue;
        }

        // Parse action
        int action_found = 0;
        for (size_t k = 0; k < sizeof(INST_ACTION_STRINGS) / sizeof(char*); k++) {
            if (str_begins_with(input_buf + i, INST_ACTION_STRINGS[k])) {
                action_found = 1;
                // TODO precompile lengths so we don't have to use strlen here
                i += strlen(INST_ACTION_STRINGS[k]);
                instructions[inst_count].action = k;
                continue;
            }
        }

        // No action means invalid instruction
        if (!action_found) {
            printf("Invalid instruction on line %li\n", src_line_count + 1);
            return 1;
        }

        // Skip whitespace
        while (input_buf[i] == ' ' || input_buf[i] == '\t')
            i++;

        // Parse memory segment
        int segment_found = 0;
        for (size_t k = 0; k < sizeof(INST_SEGMENT_STRINGS) / sizeof(char*); k++) {
            if (str_begins_with(input_buf + i, INST_SEGMENT_STRINGS[k])) {
                segment_found = 1;
                // TODO precompile lengths so we don't have to use strlen here
                i += strlen(INST_SEGMENT_STRINGS[k]);
                instructions[inst_count].segment = k;
                continue;
            }
        }

        // Skip whitespace
        while (input_buf[i] == ' ' || input_buf[i] == '\t')
            i++;

        // Parse number (only valid when segment found)
        if (is_number(input_buf[i]) && segment_found) {
            // Find end of number token
            char *num_end = input_buf + i + 1;
            while (is_number(*num_end))
                num_end++;
            num_end--; // Stand on last digit of number

            // Parse the number and add to instruction
            instructions[inst_count].number = parse_next_int(input_buf + i, num_end);

            // Find end of line
            char *line_end = find_next_any(input_buf + i, "\r\n");
            char *inst_end = line_end;

            // Find comment between token and end of line
            char *comment_start = strstr_range(input_buf + i, "//", inst_end - 1);
            if (comment_start != NULL) {
                // Parse from start of line until comment start
                inst_end = comment_start;
            }

            // Check for invalid chars between num_end and end
            num_end++; // Stand on char after number
            while (num_end < inst_end) {
                // Only whitespace is valid
                if (*num_end != ' ' && *num_end != '\t') {
                    printf("Invalid char '%c' on line %li\n", *num_end, src_line_count);
                    return 1;
                }
                num_end++;
            }

            // Move i to the end of the line
            i = line_end - input_buf + 1;
        }

        inst_count++;
    }

    size_t out_buf_size = INITIAL_HACK_CODE_SIZE_PER_INST * inst_count * sizeof(char);
    char *out_buf = malloc(out_buf_size);
    size_t out_i = 0; // index of last written char in out_buf
    // Generate code for each instruction
    for (size_t i = 0; i < inst_count; i++) {
        int line_size = 0;

        // Keep resizing output_buf until new instruction fits
        while(1) {
            line_size = snprintf_instruction(out_buf + out_i, out_buf_size - out_i,
                instructions + i);
            // If out_buf is full, reallocate and restart loop
            if (line_size > out_buf_size - out_i) {
                out_buf_size *= 2;
                out_buf = realloc(out_buf, out_buf_size);
                continue;
            }
            out_i += line_size;
            break;
        }

        out_buf[out_i++] = '\n';
    }

    out_buf[++out_i] = '\0';

    printf("%s", out_buf);

    /*char *out_buf = "@10\n"
                       "D=A\n"
                       "@SP\n"
                       "A=M\n"
                       "M=D\n"
                       "@SP\n"
                       "M=M+1\n";*/

    // Print the file to stdout
    //printf("\nInput is:\n%s\nOutput is:\n%s\n", input_buf, output_buf);
    return 0;
}
