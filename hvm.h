#ifndef HVM_H
#define HVM_H

enum INST_TYPE {
    INST_ARITHLOGIC = 0,
    INST_STACK,
    INST_FLOW,
    INST_FUNC,
};

enum ARITHLOGIC_ACTION {
    ADD = 0,
    SUB, NEG,
    EQ,  GT,  LT,
    AND, OR,  NOT,
    //ARITHLOGIC_ACTION_PARSE_ERROR,
};

char *ARITHLOGIC_ACTION_STRINGS[] = {
    [ADD] = "add",
    [SUB] = "sub", [NEG] = "neg",
    [EQ]  = "eq",  [GT]  = "gt",  [LT]  = "lt",
    [AND] = "and", [OR]  = "or",  [NOT] = "not",
};

char *ARITHLOGIC_ACTION_TABLE[] = {
    [ADD] = "+",
    [SUB] = "-",   [NEG] = "-",
    [EQ]  = "JEQ", [GT]  = "JGT", [LT]  = "JLT",
    [AND] = "&",   [OR]  = "|",   [NOT] = "!",
};

enum STACK_ACTION {
    POP = 0, PUSH,
    //STACK_ACTION_PARSE_ERROR,
};

char *STACK_ACTION_STRINGS[] = {
    [POP] = "pop", [PUSH] = "push",
};

enum SEGMENT {
    SEG_NONE = 0,
    SEG_ARGUMENT, SEG_LOCAL,
    SEG_STATIC,   SEG_CONSTANT,
    SEG_THIS,     SEG_THAT,
    SEG_POINTER,  SEG_TEMP,
    //SEG_PARSE_ERROR,
};

char *SEGMENT_STRINGS[] = {
    [SEG_NONE]     = "NONE",
    [SEG_ARGUMENT] = "argument",  [SEG_LOCAL]    = "local",
    [SEG_STATIC]   = "static",    [SEG_CONSTANT] = "constant",
    [SEG_THIS]     = "this",      [SEG_THAT]     = "that",
    [SEG_POINTER]  = "pointer",   [SEG_TEMP]     = "temp",
};

char *SEGMENT_TO_REGISTER_NAME[] = {
    [SEG_NONE]     = "NONE",
    [SEG_ARGUMENT] = "ARG",     [SEG_LOCAL]    = "LCL",
    [SEG_STATIC]   = "NONE",    [SEG_CONSTANT] = "NONE",
    [SEG_THIS]     = "THIS",    [SEG_THAT]     = "THAT",
    [SEG_POINTER]  = "POINTER", [SEG_TEMP]     = "5",
};

enum FLOW_ACTION {
    DECLARE_LABEL = 0,
    GOTO, IF_GOTO
};

char *FLOW_ACTION_STRINGS[] = {
    [DECLARE_LABEL] = "label",
    [GOTO]          = "goto",
    [IF_GOTO]       = "if-goto",
};

enum FUNC_ACTION {
    DECLARE_FUNC = 0,
    CALL, RETURN
};

char *FUNC_ACTION_STRINGS[] = {
    [DECLARE_FUNC] = "function",
    [CALL]         = "call",
    [RETURN]       = "return",
};

typedef struct {
    enum ARITHLOGIC_ACTION action;
} Arithlogic_Instruction;

typedef struct {
    enum STACK_ACTION action;
    enum SEGMENT segment;
    int number;
} Stack_Instruction;

typedef struct {
    enum FLOW_ACTION action; // DECLARE, GOTO, IF_GOTO
    char *label_name; // can not be null
} Flow_Instruction;

typedef struct {
    enum FUNC_ACTION action; // DECLARE, CALL, RETURN
    char *func_name; // can be null
    int number; // when declaring, n local vars; when calling, n args passed;
} Func_Instruction;

typedef struct {
    enum INST_TYPE type;
    union {
        Arithlogic_Instruction arithlogic;
        Stack_Instruction stack;
        Flow_Instruction flow;
        Func_Instruction func;
    } inst;
} Instruction;

#endif // HVM_H