#ifndef TYPES_H
#define TYPES_H

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_VOID
} DataType;

typedef union {
    int i_val;
    double f_val;
    char *s_val;
} ValueData;

static inline const char* type_to_rust(DataType type) {
    switch (type) {
        case TYPE_INT:    return "i32";
        case TYPE_FLOAT:  return "f32";
        case TYPE_DOUBLE: return "f64";
        case TYPE_CHAR:   return "char";
        case TYPE_VOID:   return "()";
        default:          return "i32";
    }
}

#endif