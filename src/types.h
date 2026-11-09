#ifndef TYPES_H
#define TYPES_H

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_VOID,
    /* marcas da anotacao de tipos na arvore (#44), nao sao tipos de C:
       NONE = o no nao e uma expressao; UNKNOWN = tipo ainda desconhecido
       (chamada a funcao definida mais adiante); ERROR = erro de tipo ja
       reportado, que nao deve gerar erro em cascata */
    TYPE_NONE,
    TYPE_UNKNOWN,
    TYPE_ERROR
} DataType;

/* valor de um token. Nos numeros o lexer preenche o valor (i_val/f_val) e
   tambem o lexema (s_val), que vai pra folha da arvore igual ao fonte */
typedef struct {
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

/* nome do tipo em C, pras folhas TYPE_KW da arvore (#16) e pra tabela de
   simbolos (--symtab, #25) */
static inline const char* type_to_c(DataType type) {
    switch (type) {
        case TYPE_INT:    return "int";
        case TYPE_FLOAT:  return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_CHAR:   return "char";
        case TYPE_VOID:   return "void";
        case TYPE_UNKNOWN: return "?";
        case TYPE_ERROR:  return "erro";
        default:          return "int";
    }
}

#endif