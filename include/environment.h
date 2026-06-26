#ifndef NEXO_ENVIRONMENT_H
#define NEXO_ENVIRONMENT_H

#include "value.h"

typedef struct Environment Environment;

struct Environment
{
  char **names;
  Value *values;
  int count;
  int capacity;
  Environment *parent; // escopo externo (NULL no global)
};

Environment *env_new(Environment *parent);
void env_free(Environment *env);

// Define uma nova variável no escopo atual
void env_define(Environment *env, const char *name, Value value);

// Busca uma variável (sobe pelos escopos). Retorna 1 se achou.
int env_get(Environment *env, const char *name, Value *out);

// Atribui a uma variável existente (sobe pelos escopos). 1 se achou.
int env_assign(Environment *env, const char *name, Value value);

#endif