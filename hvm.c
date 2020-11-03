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

#define COMMENT_HEADERS_IN_OUTFILE         1

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

    // Completely read file into a buffer
    /*size_t input_file_size;
    char *input_buf = load_file(input_file_path, &input_file_size);*/

    char *input_buf = "push constant 10\n";


    size_t inst_count = 0;
    size_t src_line_count = 0; // for pointing out errors
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
        
        // Handle comment
        if (input_buf[i] == '/' && input_buf[i+1] == '/') {
            // Skip rest of line
            i = find_next_any_index(input_buf, i, "\r\n") + 1;
            continue;
        }

        // Handle pop instruction
        if (strindex(input_buf + i, "pop") == 0) {
            i += 3;

            // Skip whitespace
            while (input_buf[i] == ' ' || input_buf[i] == '\t')
                i++;
            
            // Find end of line
            char *end = find_next_any(input_buf + i, "\r\n");

             // Find comment between token and end of line
            char *comment_start = strstr_range(input_buf + i, "//", end - 1);
            if (comment_start != NULL) {
                // Parse from start of line until comment start
                end = comment_start;
            }

            // TODO parse pop instruction here

            inst_count++;

            // Skip rest of line
            i = find_next_any_index(input_buf, i, "\r\n") + 1;
            continue;
        }

        // Handle push instruction
        if (strindex(input_buf + i, "push") == 0) {
            i += 4;

            // Skip whitespace
            while (input_buf[i] == ' ' || input_buf[i] == '\t')
                i++;

            // Find end of line
            char *end = find_nex_any(input_buf + i, "\r\n");

            // Find comment between token and end of line
            char *comment_start = strstr_range(input_buf + i, "//", end - 1);
            continue;
        }
        
        // No push or pop instruction, give error
        printf("Invalid instruction on line %li\n", src_line_count + 1);
        return 1;
    }

    char *output_buf = "@10\n"
                       "D=A\n"
                       "@SP\n"
                       "A=M\n"
                       "M=D\n"
                       "@SP\n"
                       "M=M+1\n";

    // Print the file to stdout
    printf("\nInput is:\n%s\nOutput is:\n%s\n", input_buf, output_buf);

    return 0;
}
