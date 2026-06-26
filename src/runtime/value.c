#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Value value_null(void)
{
  Value v;
  v.type = VAL_NULL;
  return v;
}
Value value_int(long long x)
{
  Value v;
  v.type = VAL_INT;
  v.as.int_val = x;
  return v;
}
Value value_float(double x)
{
  Value v;
  v.type = VAL_FLOAT;
  v.as.float_val = x;
  return v;
}
Value value_bool(int x)
{
  Value v;
  v.type = VAL_BOOL;
  v.as.bool_val = x ? 1 : 0;
  return v;
}

Value value_string_len(const char *chars, int n)
{
  ObjString *s = (ObjString *)malloc(sizeof(ObjString));
  s->chars = (char *)malloc(n + 1);
  memcpy(s->chars, chars, n);
  s->chars[n] = '\0';
  s->length = n;
  Value v;
  v.type = VAL_STRING;
  v.as.string = s;
  return v;
}
Value value_string(const char *chars)
{
  return value_string_len(chars, (int)strlen(chars));
}

Value value_array(void)
{
  ObjArray *a = (ObjArray *)malloc(sizeof(ObjArray));
  a->items = NULL;
  a->count = 0;
  a->capacity = 0;
  Value v;
  v.type = VAL_ARRAY;
  v.as.array = a;
  return v;
}
Value value_object(void)
{
  ObjObject *o = (ObjObject *)malloc(sizeof(ObjObject));
  o->keys = NULL;
  o->values = NULL;
  o->count = 0;
  o->capacity = 0;
  Value v;
  v.type = VAL_OBJECT;
  v.as.object = o;
  return v;
}

void array_push(ObjArray *arr, Value v)
{
  if (arr->count + 1 > arr->capacity)
  {
    arr->capacity = arr->capacity < 8 ? 8 : arr->capacity * 2;
    arr->items = realloc(arr->items, sizeof(Value) * arr->capacity);
  }
  arr->items[arr->count++] = v;
}

void object_set(ObjObject *obj, const char *key, Value v)
{
  // Se a chave já existe, sobrescreve
  for (int i = 0; i < obj->count; i++)
  {
    if (strcmp(obj->keys[i], key) == 0)
    {
      obj->values[i] = v;
      return;
    }
  }
  if (obj->count + 1 > obj->capacity)
  {
    obj->capacity = obj->capacity < 8 ? 8 : obj->capacity * 2;
    obj->keys = realloc(obj->keys, sizeof(char *) * obj->capacity);
    obj->values = realloc(obj->values, sizeof(Value) * obj->capacity);
  }
  obj->keys[obj->count] = strdup(key);
  obj->values[obj->count] = v;
  obj->count++;
}
int object_get(ObjObject *obj, const char *key, Value *out)
{
  for (int i = 0; i < obj->count; i++)
  {
    if (strcmp(obj->keys[i], key) == 0)
    {
      *out = obj->values[i];
      return 1;
    }
  }
  return 0;
}

int value_is_truthy(Value v)
{
  switch (v.type)
  {
  case VAL_NULL:
    return 0;
  case VAL_BOOL:
    return v.as.bool_val;
  case VAL_INT:
    return v.as.int_val != 0;
  case VAL_FLOAT:
    return v.as.float_val != 0.0;
  case VAL_STRING:
    return v.as.string->length > 0;
  default:
    return 1; // arrays, objetos, funções são "truthy"
  }
}

int value_equals(Value a, Value b)
{
  if (a.type != b.type)
  {
    // comparação numérica entre int e float
    if ((a.type == VAL_INT && b.type == VAL_FLOAT))
      return (double)a.as.int_val == b.as.float_val;
    if ((a.type == VAL_FLOAT && b.type == VAL_INT))
      return a.as.float_val == (double)b.as.int_val;
    return 0;
  }
  switch (a.type)
  {
  case VAL_NULL:
    return 1;
  case VAL_INT:
    return a.as.int_val == b.as.int_val;
  case VAL_FLOAT:
    return a.as.float_val == b.as.float_val;
  case VAL_BOOL:
    return a.as.bool_val == b.as.bool_val;
  case VAL_STRING:
    return a.as.string->length == b.as.string->length &&
           memcmp(a.as.string->chars, b.as.string->chars,
                  a.as.string->length) == 0;
  default:
    return a.as.object == b.as.object; // identidade
  }
}

const char *value_type_name(ValueType type)
{
  switch (type)
  {
  case VAL_NULL:
    return "null";
  case VAL_INT:
    return "int";
  case VAL_FLOAT:
    return "float";
  case VAL_BOOL:
    return "bool";
  case VAL_STRING:
    return "string";
  case VAL_ARRAY:
    return "array";
  case VAL_OBJECT:
    return "object";
  case VAL_FUNCTION:
    return "function";
  default:
    return "unknown";
  }
}

ObjString *value_to_string(Value v)
{
  char buffer[64];
  switch (v.type)
  {
  case VAL_NULL:
    return value_string("null").as.string;
  case VAL_INT:
    snprintf(buffer, sizeof(buffer), "%lld", v.as.int_val);
    return value_string(buffer).as.string;
  case VAL_FLOAT:
    snprintf(buffer, sizeof(buffer), "%g", v.as.float_val);
    return value_string(buffer).as.string;
  case VAL_BOOL:
    return value_string(v.as.bool_val ? "true" : "false").as.string;
  case VAL_STRING:
    return v.as.string;
  case VAL_ARRAY:
    return value_string("[array]").as.string;
  case VAL_OBJECT:
    return value_string("[object]").as.string;
  case VAL_FUNCTION:
    return value_string("[function]").as.string;
  default:
    return value_string("").as.string;
  }
}

void value_print(Value v)
{
  switch (v.type)
  {
  case VAL_NULL:
    printf("null");
    break;
  case VAL_INT:
    printf("%lld", v.as.int_val);
    break;
  case VAL_FLOAT:
    printf("%g", v.as.float_val);
    break;
  case VAL_BOOL:
    printf("%s", v.as.bool_val ? "true" : "false");
    break;
  case VAL_STRING:
    printf("%.*s", v.as.string->length, v.as.string->chars);
    break;
  case VAL_ARRAY:
  {
    printf("[");
    for (int i = 0; i < v.as.array->count; i++)
    {
      if (i > 0)
        printf(", ");
      value_print(v.as.array->items[i]);
    }
    printf("]");
    break;
  }
  case VAL_OBJECT:
  {
    printf("{");
    for (int i = 0; i < v.as.object->count; i++)
    {
      if (i > 0)
        printf(", ");
      printf("%s: ", v.as.object->keys[i]);
      value_print(v.as.object->values[i]);
    }
    printf("}");
    break;
  }
  case VAL_FUNCTION:
    printf("[function]");
    break;
  default:
    break;
  }
}

Value value_function(void *decl, void *closure, const char *name)
{
  ObjFunction *f = (ObjFunction *)malloc(sizeof(ObjFunction));
  f->decl = decl;
  f->closure = closure;
  f->name = name ? strdup(name) : NULL;
  Value v;
  v.type = VAL_FUNCTION;
  v.as.function = f;
  return v;
}