/* ast.h */
#ifndef AST_H
#define AST_H

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_CHAR
} DataType;

typedef union {
    int i_val;
    float f_val;
    char *s_val;
} ValueData;
#endif