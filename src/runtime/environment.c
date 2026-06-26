#include "environment.h"
#include <stdlib.h>
#include <string.h>

Environment *env_new(Environment *parent)
{
  Environment *env = (Environment *)malloc(sizeof(Environment));
  env->names = NULL;
  env->values = NULL;
  env->count = 0;
  env->capacity = 0;
  env->parent = parent;
  return env;
}

void env_free(Environment *env)
{
  for (int i = 0; i < env->count; i++)
    free(env->names[i]);
  free(env->names);
  free(env->values);
  free(env);
}

static int find_local(Environment *env, const char *name)
{
  for (int i = 0; i < env->count; i++)
    if (strcmp(env->names[i], name) == 0)
      return i;
  return -1;
}

void env_define(Environment *env, const char *name, Value value)
{
  int idx = find_local(env, name);
  if (idx >= 0)
  {
    env->values[idx] = value;
    return;
  } // redefinição local

  if (env->count + 1 > env->capacity)
  {
    env->capacity = env->capacity < 8 ? 8 : env->capacity * 2;
    env->names = realloc(env->names, sizeof(char *) * env->capacity);
    env->values = realloc(env->values, sizeof(Value) * env->capacity);
  }
  env->names[env->count] = strdup(name);
  env->values[env->count] = value;
  env->count++;
}

int env_get(Environment *env, const char *name, Value *out)
{
  for (Environment *e = env; e != NULL; e = e->parent)
  {
    int idx = find_local(e, name);
    if (idx >= 0)
    {
      *out = e->values[idx];
      return 1;
    }
  }
  return 0;
}

int env_assign(Environment *env, const char *name, Value value)
{
  for (Environment *e = env; e != NULL; e = e->parent)
  {
    int idx = find_local(e, name);
    if (idx >= 0)
    {
      e->values[idx] = value;
      return 1;
    }
  }
  return 0;
}