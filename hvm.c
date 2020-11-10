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
#define ERR_TEXT_SIZE                      200
#define FILE_PATH_SIZE                     200

#define INST_ARRAY_INITIAL_CAPACITY        1024
#define INST_ARRAY_CAPACITY_GROWTH_RATE    1024
#define GENERATE_HEADER_COMMENTS           1
#define INITIAL_HACK_CODE_SIZE_PER_INST    8

#include "hvm.h"

void print_instruction(Instruction *i)
{
    switch (i->type) {
    case INST_ARITHLOGIC:
        printf("Arithlogic_Instruction { .action=%s }",
            ARITHLOGIC_ACTION_STRINGS[i->inst.arithlogic.action]);
        break;
    case INST_STACK:
        printf("Stack_Instruction { .action=%s, segment=%s, number=%i }",
            STACK_ACTION_STRINGS[i->inst.stack.action],
            SEGMENT_STRINGS[i->inst.stack.segment],
            i->inst.stack.number);
        break;
    case INST_FLOW:
        printf("Flow_Instruction { .action=%s, .label_name=%s }",
            FLOW_ACTION_STRINGS[i->inst.flow.action],
            i->inst.flow.label_name);
        break;
    case INST_FUNC:
        printf("Func_Instruction { .action=%s, .func_name=%s, .number=%i }",
            FUNC_ACTION_STRINGS[i->inst.func.action],
            i->inst.func.func_name ? i->inst.func.func_name : "NULL",
            i->inst.func.number);
        break;
    default:
        printf("print_instruction: Invalid instruction type");
    }
}

int snprintf_instruction_comment(char *str, size_t size, Instruction *i)
{
    switch (i->type) {
    case INST_ARITHLOGIC:
        return snprintf(str, size,
            "// Arithlogic_Instruction { .action=%s }",
            ARITHLOGIC_ACTION_STRINGS[i->inst.arithlogic.action]);
     case INST_STACK:
        return snprintf(str, size,
            "// Stack_Instruction { .action=%s, .segment=%s, .number=%i }",
            STACK_ACTION_STRINGS[i->inst.stack.action],
            SEGMENT_STRINGS[i->inst.stack.segment],
            i->inst.stack.number);
     case INST_FLOW:
        return snprintf(str, size,
            "// Flow_Instruction { .action=%s, .label_name='%s' }",
            FLOW_ACTION_STRINGS[i->inst.flow.action],
            i->inst.flow.label_name);
    case INST_FUNC:
        return snprintf(str, size,
            "// Func_Instruction { .action=%s, .func_name='%s', .number=%i }",
            FUNC_ACTION_STRINGS[i->inst.func.action],
            i->inst.func.func_name ? i->inst.func.func_name : "NULL",
            i->inst.func.number);
    default:
        return snprintf(str, size,
            "// print_instruction: Invalid instruction type");
    }
}

// String, but defined by a range in memory (inclusive)
// Doesn't have to be null-terminated
typedef struct {
    char *start; // inclusive
    char *end; // inclusive
} Slice;

void print_str_range(char *start, char *end)
{
    if (start == NULL)
        printf("print_str_range: start is null\n");

    if (end == NULL)
        printf("print_str_range: end is null\n");

    if (start == NULL || end == NULL)
        return;

    while (start != end) {
        putchar(*start);
        start++;
    }

    putchar(*end);
    putchar('\n');
}

inline int is_number(char c)
{
    return c >= '0' && c <= '9';
}

inline int is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Checks if char is allowed to be the first char of a symbol
int is_valid_symbol_head(char c)
{
    return is_alpha(c) || c == '_' || c == '.' || c == '$' || c  == ':';
}

// Checks if char is allowed in a symbol as a non-first char
int is_valid_symbol_tail(char c)
{
    return is_valid_symbol_head(c) || is_number(c);
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
            if ((t == t_start && *s == *t) || t_end == t_start) // Return index
                return s - s_start + ((t_end == t_start) ? 1 : 0);
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

// Returned by parse_*_instruction_* functions
typedef struct {
    size_t token_length;
    char *error;
} Parse_Result;

// Expects first token to already be parsed into i.
// Parses the rest of the instruction and returns its length.
Parse_Result parse_stack_instruction_tail(char *str, Instruction *i)
{
    size_t len = 0;

    // Skip whitespace
    while (str[len] == ' ' || str[len] == '\t')
        len++;

    int segment_found = 0;
    for (size_t k = 0; k < sizeof(SEGMENT_STRINGS) / sizeof(char*); k++) {
        if (str_begins_with(str + len, SEGMENT_STRINGS[k])) {
            segment_found = 1;
            i->inst.stack.segment = k;
            len += strlen(SEGMENT_STRINGS[k]);
            break;
        }
    }

    if (!segment_found) {
        return (Parse_Result) {
            .token_length = len,
            .error = "Expected segment in stack instruction\n"
        };
    }

    // Skip whitespace
    while (str[len] == ' ' || str[len] == '\t')
        len++;

    // Parse number
    if (is_number(str[len])) {
        // Find end of number token
        char *num_end = str + len + 1;
        while (is_number(*num_end))
            num_end++;
        num_end--; // Stand on last digit of number

        // Parse the number and add to instruction
        i->inst.stack.number = parse_next_int(str + len, num_end);

        size_t num_len = (num_end + 1) - (str + len);
        len += num_len;
    } else {
        return (Parse_Result) {
            .token_length = len,
            .error = "Expected number in stack instruction\n"
        };
    }

    return (Parse_Result) { .token_length = len, .error = NULL };
}

// Parses label name, returns its length or error
Parse_Result parse_label_name(char *str)
{
    // Return error if invalid char found
    if (!is_valid_symbol_head(*str)) {
        return (Parse_Result) {
            .token_length = 0,
            .error = "Expected label name, got invalid character\n"
        };
    }

    // Find end of label name
    char *label_name_end = str + 1;
    while (is_valid_symbol_tail(*label_name_end)) {
        label_name_end++;
    }

    return (Parse_Result) { .token_length = label_name_end - str, .error = NULL };
}

// Expects first token to already be parsed into i.
// Parses the rest of the instruction and returns its length.
Parse_Result parse_flow_instruction_tail(char *str, Instruction *i)
{
    size_t len = 0;

    // Skip whitespace
    while (str[len] == ' ' || str[len] == '\t')
        len++;

    // Parse label name
    Parse_Result res = parse_label_name(str + len);
    if (res.error) {
        return (Parse_Result) {
            .token_length = len + res.token_length,
            .error = res.error
        };
    }

    // Copy label name
    size_t label_len = res.token_length;
    i->inst.flow.label_name = strncpy(malloc(sizeof(char) * label_len),
        str + len, label_len);

    len += label_len;
    return (Parse_Result) { .token_length = len, .error = NULL };
}

// Expects first token to already be parsed into i.
// Parses the rest of the instruction and returns its length.
Parse_Result parse_func_instruction_tail(char *str, Instruction *i)
{
    size_t len = 0;

    // Skip whitespace
    while (str[len] == ' ' || str[len] == '\t')
        len++;

    if (i->inst.func.action == RETURN) {
        // Nothing else to parse
        return (Parse_Result) { .token_length = len, .error = NULL };
    }

    // Parse function name
    Parse_Result res = parse_label_name(str + len);
    if (res.error) {
        return (Parse_Result) {
            .token_length = len + res.token_length,
            .error = res.error
        };
    }

    // Copy function name
    size_t func_len = res.token_length;
    i->inst.func.func_name = strncpy(malloc(sizeof(char) * func_len),
        str + len, func_len);

    len += func_len;

    // Skip whitespace
    while (str[len] == ' ' || str[len] == '\t')
        len++;

    // Parse number
    if (is_number(str[len])) {
        // Find end of number token
        char *num_end = str + len + 1;
        while (is_number(*num_end))
            num_end++;
        num_end--; // Stand on last digit of number

        // Parse the number and add to instruction
        i->inst.func.number = parse_next_int(str + len, num_end);

        size_t num_len = (num_end + 1) - (str + len);
        len += num_len;
    } else {
        return (Parse_Result) {
            .token_length = len,
            .error = "Expected number in func instruction\n"
        };
    }
    
    return (Parse_Result) { .token_length = len, .error = NULL };    
}

typedef struct {
    int input_file_count;
    int output_file_count;
    char **input_files; // input_files[k] is compiled into output_files[k]
    char **output_files; // if output_file_count == 1, all input_files compile into one
    char *error;
} Argparse_Result;

// Parses CLI arguments
Argparse_Result parse_arguments(int argc, char *argv[])
{
    printf("args are:\n");
    for (int i = 0; i < argc; i++) {
        printf("%s\n", argv[i]);
    }
    printf("\n");

    Argparse_Result r = {
        .input_file_count = 0,
        .output_file_count = 0,
        .input_files = NULL,
        .output_files = NULL,
        .error = NULL
    };

    if (argc < MIN_ARGC) {
        r.error = "Expected input file\n";
        return r;
    }

    int output_switch = 0;
    for (int i = 1; i < argc; i++) {
        // Handle -o switch
        if (str_begins_with(argv[i], "-o")) {
            if (output_switch) {
                r.error = "Multiple -o switches not allowed\n";
                return r;
            }

            if (argv[i][2] != 0) {
                r.error = "Unrecognized argument following -o\n";
                return r;
            }

            // Grab next arg as single output file
            if (i + 1 >= argc) {
                r.error = "Expected output file after '-o'\n";
                return r;
            }

            i++;
            // Add output file
            r.output_files = malloc(sizeof(char**));
            size_t len = strlen(argv[i]);
            r.output_files[r.output_file_count++] = strcpy(malloc(len * sizeof(char)), argv[i]);

            output_switch = 1;
            continue;
        }

        // Add input file
        r.input_files = realloc(r.input_files, sizeof(char**) * (++r.input_file_count));
        size_t len = strlen(argv[i]);
        r.input_files[r.input_file_count-1] = strcpy(malloc(len * sizeof(char)), argv[i]);
    }

    if (r.input_file_count == 0) {
        r.error = "No input file given\n";
        return r;
    }

    // Output switch not given, we have as many output files as input files
    if (!output_switch) {
        // Copy input files to output files, replacing extensions
        r.output_file_count = r.input_file_count;
        r.output_files = realloc(r.output_files, sizeof(char**) * r.output_file_count);
        for (int i = 0; i < r.input_file_count; i++) {
            // Copy string
            size_t in_len = strlen(r.input_files[i]);
            size_t len = in_len - strlen(".vm") + strlen(".asm");
            if (len < in_len) len = in_len; // Make sure
            r.output_files[i] = strcpy(malloc(len * sizeof(char)), r.input_files[i]);

            // Replace extension
            int err = str_replace_last(r.output_files[i], ".vm", ".asm");
            if (err != 0) {
                // input file doesn't end with ".vm"
                strcat(r.output_files[i], ".asm");
            }
        }
    }

    return r;
}

typedef struct {
    size_t instruction_count;
    char *output_buf;
    size_t output_buf_size;
} Trans_Result;

Trans_Result translate(char *input_buf)
{
    return (Trans_Result) { .instruction_count = 0 };
}

int main(int argc, char* argv[])
{
    Argparse_Result r = parse_arguments(argc, argv);
    if (r.error) {
        printf("Error parsing arguments: %s", r.error);
        return 1;
    }

    printf("input_files: (%i)\n", r.input_file_count);
    for (int i = 0; i < r.input_file_count; i++)
        printf("\t%s\n", r.input_files[i]);

    printf("output_files: (%i)\n", r.output_file_count);
    for (int i = 0; i < r.output_file_count; i++)
        printf("\t%s\n", r.output_files[i]);

    // Determine input file basenames TODO move into translate func
    char **input_file_basenames = malloc(sizeof(char**) * r.input_file_count);
    for (int k = 0; k < r.input_file_count; k++) {
        int last_slash_i = strindex_last(r.input_files[k], "/");
        int basename_len = strlen(r.input_files[k] - last_slash_i);
        input_file_basenames[k] = malloc(basename_len * sizeof(char));

        // Copy from last_slash_i + 1 to the end
        for (int i = 0, j = last_slash_i + 1; r.input_files[k][j-1] != '\0'; i++, j++)
            input_file_basenames[k][i] = r.input_files[k][j];
    }

    // Translate all files
    size_t *input_buf_sizes = malloc(r.input_file_count * sizeof(size_t));
    char **input_bufs = malloc(r.input_file_count * sizeof(char**));
    char **output_bufs = malloc(r.output_file_count * sizeof(char**));
    Trans_Result *trs = malloc(r.input_file_count * sizeof(Trans_Result));
    for (int i = 0; i < r.input_file_count; i++) {
        input_bufs[i] = load_file(r.input_files[i], &input_buf_sizes[i]);
        trs[i] = translate(input_bufs[i]);
    }

    // Concatenate into one file if single output file
    if (r.output_file_count == 1) {
        // Get total output size
        size_t output_buf_size = 0;
        for (int i = 0; i < r.input_file_count; i++) {
            output_buf_size += (trs[i].output_buf_size - 1); // Exclude nullterms
        }
        output_buf_size += 1; // nullterm

        // Concatenate them all
        output_bufs[0] = malloc(output_buf_size * sizeof(char*));
        size_t curr_len = 0;
        for (int i = 0; i < r.input_file_count; i++) {
            strcat(output_bufs[0] + curr_len, trs[i].output_buf);
            curr_len += output_buf_size - 1; // Overwrite nullterm
        }

        // Update size inside translate result object
        trs[0].output_buf_size = output_buf_size;
    }

    // Write out all files
    for (int i = 0; i < r.output_file_count; i++) {
        int error = write_file(output_bufs[i], r.output_files[i], trs[i].output_buf_size - 1);
        if (error) {
            printf("Error when writing to '%s'\n", r.output_files[i]);
            return 1;
        }
    }

    // Free memory TODO lmao

    return 0;
}

/*    size_t instructions_capacity = INST_ARRAY_INITIAL_CAPACITY;
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
*/
        /* Parse one of the following to determine instruction type:
            add/sub/neg/eq/gt/lt/and/or/not - arithlogic
            push/pop - stack
            label/goto/if-goto - flow
            function/call/return - func */

/*        Parse_Result res = { .token_length = 0, .error = NULL };

        // Arithlogic instruction
        for (size_t k = 0; k < sizeof(ARITHLOGIC_ACTION_STRINGS) / sizeof(char*); k++) {
            if (str_begins_with(input_buf + i, ARITHLOGIC_ACTION_STRINGS[k])) {
                i += strlen(ARITHLOGIC_ACTION_STRINGS[k]);
                instructions[inst_count].type = INST_ARITHLOGIC;
                instructions[inst_count].inst.arithlogic.action = k;
                // Arithlogic instruction only has one token, which we parsed above
                res.token_length = 0;
                res.error = NULL;
                goto instruction_parsed;
            }
        }

        // Stack instruction (pop/push)
        for (size_t k = 0; k < sizeof(STACK_ACTION_STRINGS) / sizeof(char*); k++) {
            if (str_begins_with(input_buf + i, STACK_ACTION_STRINGS[k])) {
                i += strlen(STACK_ACTION_STRINGS[k]);
                instructions[inst_count].type = INST_STACK;
                instructions[inst_count].inst.stack.action = k;
                res = parse_stack_instruction_tail(input_buf + i, instructions + inst_count);
                goto instruction_parsed;
            }
        }

        // Flow instruction
        for (size_t k = 0; k < sizeof(FLOW_ACTION_STRINGS) / sizeof(char*); k++) {
            if (str_begins_with(input_buf + i, FLOW_ACTION_STRINGS[k])) {
                i += strlen(FLOW_ACTION_STRINGS[k]);
                instructions[inst_count].type = INST_FLOW;
                instructions[inst_count].inst.flow.action = k;
                res = parse_flow_instruction_tail(input_buf + i, instructions + inst_count);
                goto instruction_parsed;
            }
        }

        // Func instruction
        for (size_t k = 0; k < sizeof(FUNC_ACTION_STRINGS) / sizeof(char*); k++) { 
            if (str_begins_with(input_buf + i, FUNC_ACTION_STRINGS[k])) {
                i += strlen(FUNC_ACTION_STRINGS[k]);
                instructions[inst_count].type = INST_FUNC;
                instructions[inst_count].inst.func.action = k;
                res = parse_func_instruction_tail(input_buf + i, instructions + inst_count);
                goto instruction_parsed;
            }
        }

        // Invalid token
        printf("Invalid first token on line %li\n", src_line_count);
        return 1;

instruction_parsed:
        // Check for parse error
        if (res.error) {
            printf("Parse error on line %li: ", src_line_count);
            printf("%s", res.error);
            return 1;
        }

        // Move to next char after token
        i += res.token_length;

        // Skip whitespace
        while (input_buf[i] == ' ' || input_buf[i] == '\t')
            i++;

        // Check for invalid chars
        char *line_end = find_next_any(input_buf + i, "\r\n");
        char *comment_start = strstr_range(input_buf + i, line_end, "//");
        char *check_until = comment_start == NULL ? line_end : comment_start;
        while (input_buf + i < check_until) {
            // Anything other than whitespace is invalid
            if (input_buf[i] != ' ' && input_buf[i] != '\t') {
                printf("Invalid char '%c' on line %li\n", input_buf[i], src_line_count);
                return 1;
            }
            i++;
        }

        // Move i to the end of the line
        i = line_end - input_buf + 1;        
        
        inst_count++;
    }

    size_t out_buf_size = INITIAL_HACK_CODE_SIZE_PER_INST * inst_count * sizeof(char);
    char *out_buf = malloc(out_buf_size);

    size_t out_i = 0; // index of last written char in out_buf
    // Generate code for each instruction
    for (size_t i = 0; i < inst_count; i++) {
        int line_size = 0;

#if GENERATE_HEADER_COMMENTS == 1
        // Keep resizing out_buf until new line of comment fits
        while(1) {
            line_size = snprintf_instruction_comment(out_buf + out_i, out_buf_size - out_i,
                instructions + i);
            // If out_buf is full, reallocate and restart loop
            if (line_size >= out_buf_size - out_i) {
                out_buf_size *= 2;
                out_buf = realloc(out_buf, out_buf_size);
                continue;
            }
            out_i += line_size;
            break;
        }
        out_buf[out_i++] = '\n';
#endif

        int append_size = 0;
        // Keep resizing out_buf until new instructions fit
        while (1) {
            switch (instructions[i].segment) {
            case SEGMENT_ARGUMENT:
            case SEGMENT_LOCAL:
            case SEGMENT_THIS:
            case SEGMENT_THAT:
            case SEGMENT_TEMP:
                if (instructions[i].action == ACTION_POP) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@%i\n"
                        "D=A\n"
                        "@%s\n"
                        "D=D+%s\n"
                        "@__loc\n"
                        "M=D\n"
                        "@SP\n"
                        "M=M-1\n"
                        "A=M\n"
                        "D=M\n"
                        "@__loc\n"
                        "A=M\n"
                        "M=D\n",
                        instructions[i].number,
                        SEGMENT_TO_REGISTER_NAME[instructions[i].segment],
                        instructions[i].segment == SEGMENT_TEMP ? "A" : "M");
                } else if (instructions[i].action == ACTION_PUSH) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@%i\n"
                        "D=A\n"
                        "@%s\n"
                        "A=D+%s\n"
                        "D=M\n"
                        "@SP\n"
                        "A=M\n"
                        "M=D\n"
                        "@SP\n"
                        "M=M+1\n",
                        instructions[i].number,
                        SEGMENT_TO_REGISTER_NAME[instructions[i].segment],
                        instructions[i].segment == SEGMENT_TEMP ? "A" : "M");
                } else {
                    printf("Invalid action-segment pair. Can only push/pop when segment specified.\n");
                    return 1;
                }
            break;

            case SEGMENT_CONSTANT:
                if (instructions[i].action == ACTION_PUSH) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@%i\n"
                        "D=A\n"
                        "@SP\n"
                        "A=M\n"
                        "M=D\n"
                        "@SP\n"
                        "M=M+1\n",
                        instructions[i].number);
                } else {
                    printf("Invalid action. Can only 'push' from '%s' memory segment.\n",
                        INST_SEGMENT_STRINGS[SEGMENT_CONSTANT]);
                    return 1;
                }
            break;

            case SEGMENT_POINTER:
                // Exit if invalid number given
                if (instructions[i].number != 0 && instructions[i].number != 1) {
                    printf("Invalid number '%i' with memory segment '%s'\n",
                        instructions[i].number,
                        INST_SEGMENT_STRINGS[SEGMENT_POINTER]);
                    return 1;
                }

                if (instructions[i].action == ACTION_PUSH) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@%s\n"
                        "D=M\n"
                        "@SP\n"
                        "A=M\n"
                        "M=D\n"
                        "@SP\n"
                        "M=M+1\n",
                        instructions[i].number == 0 ?
                            SEGMENT_TO_REGISTER_NAME[SEGMENT_THIS] : SEGMENT_TO_REGISTER_NAME[SEGMENT_THAT]);
                } else if (instructions[i].action == ACTION_POP) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@SP\n"
                        "M=M-1\n"
                        "A=M\n"
                        "D=M\n"
                        "@%s\n"
                        "M=D\n",
                        instructions[i].number == 0 ?
                            SEGMENT_TO_REGISTER_NAME[SEGMENT_THIS] : SEGMENT_TO_REGISTER_NAME[SEGMENT_THAT]);
                }
            break;

            case SEGMENT_STATIC:
                if (instructions[i].action == ACTION_PUSH) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@%s.%i\n"
                        "D=M\n"
                        "@SP\n"
                        "A=M\n"
                        "M=D\n"
                        "@SP\n"
                        "M=M+1\n",
                        input_file_name, instructions[i].number);
                } else if (instructions[i].action == ACTION_POP) {
                    append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                        "@SP\n"
                        "M=M-1\n"
                        "A=M\n"
                        "D=M\n"
                        "@%s.%i\n"
                        "M=D\n",
                        input_file_name, instructions[i].number);
                }
            break;

            case SEGMENT_NONE:
                switch (instructions[i].action) {
                    case ACTION_NEG:
                    case ACTION_NOT:
                        append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                            "@SP\n"
                            "A=M-1\n"
                            "M=%sM\n",
                            instructions[i].action == ACTION_NEG ?
                                ARITHLOGIC_ACTION_TABLE[ACTION_NEG]
                                : ARITHLOGIC_ACTION_TABLE[ACTION_NOT]);
                    break;

                    case ACTION_ADD:
                    case ACTION_SUB:
                    case ACTION_AND:
                    case ACTION_OR:
                        append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                            "@SP\n"
                            "M=M-1\n"
                            "A=M\n"
                            "D=M\n"
                            "A=A-1\n"
                            "M=M%sD\n",
                            ARITHLOGIC_ACTION_TABLE[instructions[i].action]);
                    break;

                    case ACTION_EQ:
                    case ACTION_LT:
                    case ACTION_GT: {
                        char *action_str = INST_ACTION_STRINGS[instructions[i].action];
                        char *comp_str = ARITHLOGIC_ACTION_TABLE[instructions[i].action];
                        append_size = snprintf(out_buf + out_i, out_buf_size - out_i,
                            "@SP\n"
                            "M=M-1\n"
                            "A=M\n"
                            "D=M\n"
                            "A=A-1\n"
                            "D=M-D\n"
                            "@__%s.%s.%li.T\n"
                            "D;%s\n"
                            "@__%s.%s.%li.F\n"
                            "0;JMP\n"
                            "(__%s.%s.%li.T)\n"
                            "@SP\n"
                            "A=M-1\n"
                            "M=-1\n"
                            "@__%s.%s.%li.END\n"
                            "0;JMP\n"
                            "(__%s.%s.%li.F)\n"
                            "@SP\n"
                            "A=M-1\n"
                            "M=0\n"
                            "(__%s.%s.%li.END)\n",
                            input_file_name, action_str, i,
                            comp_str,
                            input_file_name, action_str, i,
                            input_file_name, action_str, i,
                            input_file_name, action_str, i,
                            input_file_name, action_str, i,
                            input_file_name, action_str, i);
                    }
                    break;

                    default: {
                        printf("Fatal error, action is '%s'\n", INST_ACTION_STRINGS[instructions[i].action]);
                        return 1;
                    }
                }
                
                break;

            default:
                printf("Unhandled memory segment specified\n");
                append_size = 0;
                break;
            }

            // If out_buf is full, reallocate and restart loop
            if (append_size >= out_buf_size - out_i) {
                out_buf_size *= 2;
                out_buf = realloc(out_buf, out_buf_size);
                continue;
            }
            out_i += append_size;
            break;
        }
    }

    out_buf[++out_i] = '\0';

    // Write file (size - 1 to exclude null terminator)
    int error = write_file(out_buf, output_file_path, out_i - 1);
    if (error) {
        printf("Error when writing to '%s'\n", output_file_path);
    }

    // Free all memory
    free(input_buf);
    free(instructions);
    free(out_buf);
    return 0;
}
    */