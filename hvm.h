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
    //ACTION_PARSE_ERROR,
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
    //SEGMENT_PARSE_ERROR,
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

char *SEGMENT_TO_REGISTER_NAME[] = {
    [SEGMENT_NONE]     = "NONE",
    [SEGMENT_ARGUMENT] = "ARG",
    [SEGMENT_LOCAL]    = "LCL",
    [SEGMENT_STATIC]   = "NONE",
    [SEGMENT_CONSTANT] = "NONE",
    [SEGMENT_THIS]     = "THIS",
    [SEGMENT_THAT]     = "THAT",
    [SEGMENT_POINTER]  = "POINTER",
    [SEGMENT_TEMP]     = "5", // TEMP starts at address 5
};

char *ARITHMETIC_LOGIC_ACTION_TABLE[] = {
    [ACTION_ADD]  = "+",
    [ACTION_SUB]  = "-",
    [ACTION_NEG]  = "-",
    [ACTION_AND]  = "&",
    [ACTION_OR]   = "|",
    [ACTION_NOT]  = "!",
};

char *COMPARISON_ACTION_TABLE[] = {
    [ACTION_EQ] = "JEQ",
    [ACTION_GT] = "JGT",
    [ACTION_LT] = "JLT",
};
