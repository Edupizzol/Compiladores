#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tac_opt.h"

/* ---- instrucoes ----
   O TAC do tac.c e lido de volta linha a linha. As formas sao poucas:
     x = y            x = (tipo) y      x = - y / x = ! y
     x = a op b       x = call f, n     param v      return v
     goto Ln          ifFalse v goto Ln / if v goto Ln
     Ln:              func ...: / endfunc (passam direto) */

typedef enum {
    I_COPY, I_CONV, I_UNARY, I_BINARY, I_CALL, I_PARAM, I_RETURN,
    I_GOTO, I_IF, I_LABEL, I_OTHER
} Kind;

typedef struct {
    Kind kind;
    char *dst;     /* destino de COPY/CONV/UNARY/BINARY/CALL */
    char *a, *b;   /* operandos lidos (b so no BINARY) */
    char *op;      /* operador, tipo da conversao, funcao ou "if"/"ifFalse" */
    char *nargs;   /* CALL: quantos argumentos */
    int label;     /* GOTO/IF/LABEL */
    char *raw;     /* OTHER: a linha como veio */
    int dead;
} Instr;

static Instr *code;
static int ncode, capcode;

static Instr *push_instr(Kind kind) {
    if (ncode == capcode) {
        capcode = capcode ? capcode * 2 : 64;
        code = realloc(code, capcode * sizeof(Instr));
    }
    Instr *in = &code[ncode++];
    memset(in, 0, sizeof *in);
    in->kind = kind;
    return in;
}

static void set_str(char **field, const char *s) {
    char *copy = strdup(s);
    free(*field);
    *field = copy;
}

static int label_number(const char *s) { return atoi(s + 1); }   /* "L12" -> 12 */

/* le uma linha do TAC. Linha que nao tem uma das formas conhecidas vira
   OTHER e sai sem mudanca */
static void parse_line(char *line) {
    size_t len = strlen(line);

    if (strncmp(line, "    ", 4) != 0) {
        if (len > 2 && line[0] == 'L' && line[len - 1] == ':') {
            push_instr(I_LABEL)->label = label_number(line);
        } else {
            push_instr(I_OTHER)->raw = strdup(line);
        }
        return;
    }

    char *tok[8];
    int n = 0;
    char *copy = strdup(line + 4);
    for (char *p = strtok(copy, " "); p && n < 8; p = strtok(NULL, " ")) tok[n++] = p;

    Instr *in = NULL;
    if (n == 2 && strcmp(tok[0], "goto") == 0) {
        in = push_instr(I_GOTO);
        in->label = label_number(tok[1]);
    } else if (n == 4 && (strcmp(tok[0], "if") == 0 || strcmp(tok[0], "ifFalse") == 0)
               && strcmp(tok[2], "goto") == 0) {
        in = push_instr(I_IF);
        in->op = strdup(tok[0]);
        in->a = strdup(tok[1]);
        in->label = label_number(tok[3]);
    } else if (n == 2 && (strcmp(tok[0], "param") == 0 || strcmp(tok[0], "return") == 0)) {
        in = push_instr(strcmp(tok[0], "param") == 0 ? I_PARAM : I_RETURN);
        in->a = strdup(tok[1]);
    } else if (n >= 3 && strcmp(tok[1], "=") == 0) {
        if (n == 3) {
            in = push_instr(I_COPY);
            in->a = strdup(tok[2]);
        } else if (n == 4 && tok[2][0] == '(') {
            in = push_instr(I_CONV);
            in->op = strndup(tok[2] + 1, strlen(tok[2]) - 2);   /* "(float)" -> "float" */
            in->a = strdup(tok[3]);
        } else if (n == 4) {
            in = push_instr(I_UNARY);
            in->op = strdup(tok[2]);
            in->a = strdup(tok[3]);
        } else if (n == 5 && strcmp(tok[2], "call") == 0) {
            in = push_instr(I_CALL);
            in->op = strndup(tok[3], strlen(tok[3]) - 1);       /* "f," -> "f" */
            in->nargs = strdup(tok[4]);
        } else if (n == 5) {
            in = push_instr(I_BINARY);
            in->a = strdup(tok[2]);
            in->op = strdup(tok[3]);
            in->b = strdup(tok[4]);
        }
        if (in) in->dst = strdup(tok[0]);
    }
    if (!in) push_instr(I_OTHER)->raw = strdup(line);
    free(copy);
}

static void print_instr(const Instr *in, FILE *out) {
    switch (in->kind) {
    case I_COPY:   fprintf(out, "    %s = %s\n", in->dst, in->a); break;
    case I_CONV:   fprintf(out, "    %s = (%s) %s\n", in->dst, in->op, in->a); break;
    case I_UNARY:  fprintf(out, "    %s = %s %s\n", in->dst, in->op, in->a); break;
    case I_BINARY: fprintf(out, "    %s = %s %s %s\n", in->dst, in->a, in->op, in->b); break;
    case I_CALL:   fprintf(out, "    %s = call %s, %s\n", in->dst, in->op, in->nargs); break;
    case I_PARAM:  fprintf(out, "    param %s\n", in->a); break;
    case I_RETURN: fprintf(out, "    return %s\n", in->a); break;
    case I_GOTO:   fprintf(out, "    goto L%d\n", in->label); break;
    case I_IF:     fprintf(out, "    %s %s goto L%d\n", in->op, in->a, in->label); break;
    case I_LABEL:  fprintf(out, "L%d:\n", in->label); break;
    case I_OTHER:  fprintf(out, "%s\n", in->raw); break;
    }
}

static void free_instr(Instr *in) {
    free(in->dst);
    free(in->a);
    free(in->b);
    free(in->op);
    free(in->nargs);
    free(in->raw);
}

/* ---- operandos ---- */

static int is_temp(const char *s) {
    if (!s || s[0] != 't' || !s[1]) return 0;
    for (const char *p = s + 1; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static int is_literal(const char *s) {
    if (*s == '-') s++;
    return isdigit((unsigned char)*s) || *s == '.';
}

/* literal inteiro que cabe num int de C. Literal com ponto ou expoente e
   float e nao entra na aritmetica */
static int int_value(const char *s, long long *v) {
    if (!is_literal(s) || strpbrk(s, ".eE")) return 0;
    char *end;
    errno = 0;
    *v = strtoll(s, &end, 10);
    return !errno && !*end && *v >= INT_MIN && *v <= INT_MAX;
}

/* ---- passo 1: rotulos sem uso ---- */

static void drop_unused_labels(int from, int to) {
    for (int i = from; i < to; i++) {
        if (code[i].kind != I_LABEL) continue;
        int used = 0;
        for (int j = from; j < to && !used; j++)
            used = (code[j].kind == I_GOTO || code[j].kind == I_IF) && code[j].label == code[i].label;
        if (!used) code[i].dead = 1;
    }
}

/* ---- passo 2: propagacao dentro do bloco basico ----
   O que se sabe de cada nome no bloco: o valor e um literal (constante) ou
   outro nome (copia). Tudo e esquecido no comeco de cada bloco */

typedef struct {
    char *name, *value;
} Fact;

static Fact *facts;
static int nfacts, capfacts;

static void facts_clear(void) {
    for (int i = 0; i < nfacts; i++) {
        free(facts[i].name);
        free(facts[i].value);
    }
    nfacts = 0;
}

static const char *fact_of(const char *name) {
    for (int i = 0; i < nfacts; i++)
        if (strcmp(facts[i].name, name) == 0) return facts[i].value;
    return NULL;
}

/* `name` recebeu um valor novo: esquece o que se sabia dele e as copias
   que apontavam pra ele */
static void facts_kill(const char *name) {
    for (int i = 0; i < nfacts;) {
        if (strcmp(facts[i].name, name) == 0 || strcmp(facts[i].value, name) == 0) {
            free(facts[i].name);
            free(facts[i].value);
            facts[i] = facts[--nfacts];
        } else {
            i++;
        }
    }
}

static void facts_add(const char *name, const char *value) {
    if (strcmp(name, value) == 0) return;
    if (nfacts == capfacts) {
        capfacts = capfacts ? capfacts * 2 : 16;
        facts = realloc(facts, capfacts * sizeof(Fact));
    }
    facts[nfacts].name = strdup(name);
    facts[nfacts].value = strdup(value);
    nfacts++;
}

/* troca o operando pelo valor conhecido, se houver */
static void substitute(char **operand) {
    if (!*operand || is_literal(*operand)) return;
    const char *v = fact_of(*operand);
    if (v) set_str(operand, v);
}

static int fits_int(long long v) { return v >= INT_MIN && v <= INT_MAX; }

/* aritmetica inteira com as regras do constant folding da arvore (#49):
   o que seria UB ou erro em C fica como esta */
static int fold_binary(const char *op, long long a, long long b, long long *r) {
    if (strcmp(op, "+") == 0)       *r = a + b;
    else if (strcmp(op, "-") == 0)  *r = a - b;
    else if (strcmp(op, "*") == 0)  *r = a * b;
    else if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        if (b == 0 || (a == INT_MIN && b == -1)) return 0;
        *r = op[0] == '/' ? a / b : a % b;
    }
    else if (strcmp(op, "&") == 0)  *r = a & b;
    else if (strcmp(op, "|") == 0)  *r = a | b;
    else if (strcmp(op, "^") == 0)  *r = a ^ b;
    else if (strcmp(op, "<<") == 0) {
        if (b < 0 || b >= 32 || a < 0) return 0;
        *r = a << b;
    } else if (strcmp(op, ">>") == 0) {
        if (b < 0 || b >= 32) return 0;
        *r = a >> b;
    }
    else if (strcmp(op, "==") == 0) *r = a == b;
    else if (strcmp(op, "!=") == 0) *r = a != b;
    else if (strcmp(op, "<") == 0)  *r = a < b;
    else if (strcmp(op, ">") == 0)  *r = a > b;
    else if (strcmp(op, "<=") == 0) *r = a <= b;
    else if (strcmp(op, ">=") == 0) *r = a >= b;
    else return 0;
    return fits_int(*r);
}

/* se o resultado da instrucao e conhecido, ela vira `dst = literal` */
static void try_fold(Instr *in) {
    long long a, b, r;
    if (in->kind == I_UNARY && int_value(in->a, &a)) {
        if (strcmp(in->op, "!") == 0) r = !a;
        else if (strcmp(in->op, "-") == 0 && a != INT_MIN) r = -a;
        else return;
    } else if (in->kind == I_BINARY && int_value(in->a, &a) && int_value(in->b, &b)) {
        if (!fold_binary(in->op, a, b, &r)) return;
    } else {
        return;
    }
    char text[32];
    snprintf(text, sizeof text, "%lld", r);
    in->kind = I_COPY;
    set_str(&in->a, text);
    free(in->b);
    free(in->op);
    in->b = in->op = NULL;
}

static void propagate(int from, int to) {
    facts_clear();
    for (int i = from; i < to; i++) {
        Instr *in = &code[i];
        if (in->dead) continue;
        if (in->kind == I_LABEL) {          /* comeca outro bloco */
            facts_clear();
            continue;
        }
        substitute(&in->a);
        substitute(&in->b);
        try_fold(in);
        if (in->dst) {
            facts_kill(in->dst);
            if (in->kind == I_COPY) facts_add(in->dst, in->a);
        }
        /* depois de salto ou chamada comeca outro bloco */
        if (in->kind == I_GOTO || in->kind == I_IF || in->kind == I_CALL) facts_clear();
    }
    facts_clear();
}

/* ---- passo 3: temporarios mortos ---- */

static int temp_uses(int from, int to, const char *t) {
    int n = 0;
    for (int i = from; i < to; i++) {
        if (code[i].dead) continue;
        if (code[i].a && strcmp(code[i].a, t) == 0) n++;
        if (code[i].b && strcmp(code[i].b, t) == 0) n++;
    }
    return n;
}

static void drop_dead_temps(int from, int to) {
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = from; i < to; i++) {
            Instr *in = &code[i];
            if (in->dead || in->kind == I_CALL || !is_temp(in->dst)) continue;
            if (temp_uses(from, to, in->dst) == 0) {
                in->dead = 1;
                changed = 1;
            }
        }
    }
}

/* ---- passo 4: renumeracao ----
   A ordem original e mantida: o menor numero que sobrou vira 0, o seguinte
   1 e assim por diante. Se nada saiu, os nomes nao mudam */

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

/* numeros distintos e ordenados de `nums` (in-place); devolve quantos */
static int unique_sorted(int *nums, int n) {
    qsort(nums, n, sizeof(int), cmp_int);
    int k = 0;
    for (int i = 0; i < n; i++)
        if (k == 0 || nums[k - 1] != nums[i]) nums[k++] = nums[i];
    return k;
}

static int rank_of(const int *nums, int n, int v) {
    for (int i = 0; i < n; i++)
        if (nums[i] == v) return i;
    return v;
}

static void rename_temp(char **field, const int *nums, int n) {
    if (!is_temp(*field)) return;
    char text[32];
    snprintf(text, sizeof text, "t%d", rank_of(nums, n, atoi(*field + 1)));
    set_str(field, text);
}

static void renumber(int from, int to) {
    int cap = 3 * (to - from) + 1;
    int *temps = malloc(cap * sizeof(int)), *labels = malloc(cap * sizeof(int));
    int nt = 0, nl = 0;
    for (int i = from; i < to; i++) {
        Instr *in = &code[i];
        if (in->dead) continue;
        char *fields[] = {in->dst, in->a, in->b};
        for (int f = 0; f < 3; f++)
            if (is_temp(fields[f])) temps[nt++] = atoi(fields[f] + 1);
        if (in->kind == I_LABEL || in->kind == I_GOTO || in->kind == I_IF)
            labels[nl++] = in->label;
    }
    nt = unique_sorted(temps, nt);
    nl = unique_sorted(labels, nl);
    for (int i = from; i < to; i++) {
        Instr *in = &code[i];
        if (in->dead) continue;
        rename_temp(&in->dst, temps, nt);
        rename_temp(&in->a, temps, nt);
        rename_temp(&in->b, temps, nt);
        if (in->kind == I_LABEL || in->kind == I_GOTO || in->kind == I_IF)
            in->label = rank_of(labels, nl, in->label);
    }
    free(temps);
    free(labels);
}

/* ---- programa ---- */

static void optimize_function(int from, int to) {
    drop_unused_labels(from, to);
    propagate(from, to);
    drop_dead_temps(from, to);
    renumber(from, to);
}

void tac_optimize(const char *text, FILE *out) {
    char *buf = strdup(text);
    for (char *line = buf, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        else if (!*line) break;          /* o \n final nao abre outra linha */
        parse_line(line);                /* a linha vazia entre funcoes fica */
    }
    free(buf);

    /* cada funcao vai de `func ...:` ate `endfunc`; os temporarios e os
       rotulos recomecam em cada uma */
    int start = -1;
    for (int i = 0; i < ncode; i++) {
        if (code[i].kind != I_OTHER) continue;
        if (strncmp(code[i].raw, "func ", 5) == 0) start = i + 1;
        else if (strcmp(code[i].raw, "endfunc") == 0 && start >= 0) {
            optimize_function(start, i);
            start = -1;
        }
    }

    for (int i = 0; i < ncode; i++) {
        if (!code[i].dead) print_instr(&code[i], out);
        free_instr(&code[i]);
    }
    free(code);
    code = NULL;
    ncode = capcode = 0;
    free(facts);
    facts = NULL;
    capfacts = 0;
}
