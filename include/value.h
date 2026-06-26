#ifndef NEXO_VALUE_H
#define NEXO_VALUE_H

typedef enum
{
  VAL_NULL,
  VAL_INT,
  VAL_FLOAT,
  VAL_BOOL,
  VAL_STRING,
  VAL_ARRAY,
  VAL_OBJECT,
  VAL_FUNCTION,
  VAL_RETURN // valor "embrulhado" sinalizando um return em andamento
} ValueType;

typedef struct Value Value;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;
typedef struct ObjObject ObjObject;
typedef struct ObjFunction ObjFunction;

struct Value
{
  ValueType type;
  union
  {
    long long int_val;
    double float_val;
    int bool_val;
    ObjString *string;
    ObjArray *array;
    ObjObject *object;
    ObjFunction *function;
  } as;
};

// String com contagem de tamanho (permite '\0' interno e evita strlen repetido)
struct ObjString
{
  char *chars;
  int length;
};

// Array dinâmico de valores
struct ObjArray
{
  Value *items;
  int count;
  int capacity;
};

// Objeto: pares chave(string)->valor
struct ObjObject
{
  char **keys;
  Value *values;
  int count;
  int capacity;
};

// Construtores de valores (helpers para criar Value rapidamente)
Value value_null(void);
Value value_int(long long v);
Value value_float(double v);
Value value_bool(int v);
Value value_string(const char *chars); // copia a string
Value value_string_len(const char *chars, int n);
Value value_array(void);
Value value_object(void);

// Operações de array
void array_push(ObjArray *arr, Value v);

// Operações de objeto
void object_set(ObjObject *obj, const char *key, Value v);
int object_get(ObjObject *obj, const char *key, Value *out); // 1 se achou

// Igualdade e verdade (truthiness)
int value_equals(Value a, Value b);
int value_is_truthy(Value v);

// Impressão (usada pela função print do Nexo)
void value_print(Value v);

// Converte um valor para string legível (para concatenação e print)
ObjString *value_to_string(Value v);

// Nome do tipo em runtime (para mensagens de erro)
const char *value_type_name(ValueType type);

// Função definida pelo usuário.
// decl aponta para um Stmt (STMT_FUNC_DECL) — void* evita dependência circular.
// closure aponta para o Environment de definição.
struct ObjFunction
{
  void *decl;
  void *closure;
  char *name;
};

Value value_function(void *decl, void *closure, const char *name);

#endif