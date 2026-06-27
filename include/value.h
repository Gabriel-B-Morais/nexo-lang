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
  VAL_CLASS,
  VAL_INSTANCE,
  VAL_RETURN
} ValueType;

typedef struct Value Value;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;
typedef struct ObjObject ObjObject;
typedef struct ObjFunction ObjFunction;
typedef struct ObjClass ObjClass;
typedef struct ObjInstance ObjInstance;

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
    ObjClass *klass;
    ObjInstance *instance;
  } as;
};

struct ObjString
{
  char *chars;
  int length;
};

struct ObjArray
{
  Value *items;
  int count;
  int capacity;
};

struct ObjObject
{
  char **keys;
  Value *values;
  int count;
  int capacity;
};

// Função definida pelo usuário.
// decl -> Stmt* (STMT_FUNC_DECL); closure -> Environment*. void* evita dependência circular.
struct ObjFunction
{
  void *decl;
  void *closure;
  char *name;
};

// Classe em runtime. decl -> Stmt* (STMT_CLASS).
struct ObjClass
{
  char *name;
  void *decl;
  ObjClass *super; // superclasse resolvida ou NULL
  char **static_names;
  Value *static_values;
  int static_count;
  int static_capacity;
};

// Instância: campos próprios + ponteiro para a classe.
struct ObjInstance
{
  ObjClass *klass;
  char **field_names;
  Value *field_values;
  int field_count;
  int field_capacity;
};

// ----- Construtores de valores -----
Value value_null(void);
Value value_int(long long v);
Value value_float(double v);
Value value_bool(int v);
Value value_string(const char *chars);
Value value_string_len(const char *chars, int n);
Value value_array(void);
Value value_object(void);
Value value_function(void *decl, void *closure, const char *name);
Value value_class(const char *name, void *decl, ObjClass *super);
Value value_instance(ObjClass *klass);

// ----- Array -----
void array_push(ObjArray *arr, Value v);

// ----- Objeto -----
void object_set(ObjObject *obj, const char *key, Value v);
int object_get(ObjObject *obj, const char *key, Value *out);

// ----- Instância -----
void instance_set(ObjInstance *inst, const char *name, Value v);
int instance_get(ObjInstance *inst, const char *name, Value *out);

// ----- Campos estáticos de classe -----
void class_static_set(ObjClass* klass, const char* name, Value v);
int  class_static_get(ObjClass* klass, const char* name, Value* out);

// ----- Utilitários -----
int value_equals(Value a, Value b);
int value_is_truthy(Value v);
void value_print(Value v);
ObjString *value_to_string(Value v);
const char *value_type_name(ValueType type);

#endif