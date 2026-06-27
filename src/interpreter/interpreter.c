#include "interpreter.h"
#include "value.h"
#include "environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  Environment *globals;
  int had_error;
  int returning;
  Value return_value;
} Interp;

static Interp interp;

static void runtime_error(int line, const char *msg)
{
  fprintf(stderr, "[linha %d] Erro de execucao: %s\n", line, msg);
  interp.had_error = 1;
}

// ----- Forward declarations -----
static Value eval_expr(Expr *expr, Environment *env);
static void exec_stmt(Stmt *stmt, Environment *env);
static void exec_block(Stmt *block, Environment *env);

static ObjClass *find_class(Environment *env, const char *name);
static Member *find_method(ObjClass *klass, const char *name);
static Value call_user_function(Stmt *decl, Environment *closure, Value *args, int arg_count);
static Value call_method(Member *m, Environment *closure, Value this_val, Value *args, int arg_count);

// =================================================================
//  HELPERS DE OOP
// =================================================================
static ObjClass *find_class(Environment *env, const char *name)
{
  Value v;
  if (env_get(env, name, &v) && v.type == VAL_CLASS)
    return v.as.klass;
  return NULL;
}

static Member *find_method(ObjClass *klass, const char *name)
{
  for (ObjClass *k = klass; k != NULL; k = k->super)
  {
    Stmt *decl = (Stmt *)k->decl;
    for (int i = 0; i < decl->as.class_decl.member_count; i++)
    {
      Member *m = decl->as.class_decl.members[i];
      if (m->is_method && strcmp(m->name, name) == 0)
        return m;
    }
  }
  return NULL;
}

static Value call_user_function(Stmt *decl, Environment *closure, Value *args, int arg_count)
{
  Environment *call_env = env_new(closure);
  int param_count = decl->as.func_decl.param_count;
  for (int i = 0; i < param_count; i++)
  {
    Value arg = (i < arg_count) ? args[i] : value_null();
    env_define(call_env, decl->as.func_decl.params[i].name, arg);
  }
  exec_block(decl->as.func_decl.body, call_env);
  Value result = value_null();
  if (interp.returning)
  {
    result = interp.return_value;
    interp.returning = 0;
  }
  env_free(call_env);
  return result;
}

static Value call_method(Member *m, Environment *closure, Value this_val, Value *args, int arg_count)
{
  Environment *call_env = env_new(closure);
  env_define(call_env, "this", this_val);
  for (int i = 0; i < m->param_count; i++)
  {
    Value arg = (i < arg_count) ? args[i] : value_null();
    env_define(call_env, m->params[i].name, arg);
  }
  exec_block(m->body, call_env);
  Value result = value_null();
  if (interp.returning)
  {
    result = interp.return_value;
    interp.returning = 0;
  }
  env_free(call_env);
  return result;
}

// =================================================================
//  EXPRESSÕES — operadores
// =================================================================
static Value eval_add(Value a, Value b, int line)
{
  if (a.type == VAL_STRING || b.type == VAL_STRING)
  {
    ObjString *sa = value_to_string(a);
    ObjString *sb = value_to_string(b);
    int len = sa->length + sb->length;
    char *buf = (char *)malloc(len + 1);
    memcpy(buf, sa->chars, sa->length);
    memcpy(buf + sa->length, sb->chars, sb->length);
    buf[len] = '\0';
    Value r = value_string_len(buf, len);
    free(buf);
    return r;
  }
  if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)
  {
    double x = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
    double y = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
    return value_float(x + y);
  }
  if (a.type == VAL_INT && b.type == VAL_INT)
    return value_int(a.as.int_val + b.as.int_val);
  runtime_error(line, "Operandos invalidos para '+'");
  return value_null();
}

static Value eval_arith(Value a, Value b, TokenType op, int line)
{
  if ((a.type != VAL_INT && a.type != VAL_FLOAT) ||
      (b.type != VAL_INT && b.type != VAL_FLOAT))
  {
    runtime_error(line, "Operandos devem ser numericos");
    return value_null();
  }
  int both_int = (a.type == VAL_INT && b.type == VAL_INT);
  double x = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
  double y = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
  switch (op)
  {
  case TOKEN_MINUS:
    return both_int ? value_int(a.as.int_val - b.as.int_val) : value_float(x - y);
  case TOKEN_STAR:
    return both_int ? value_int(a.as.int_val * b.as.int_val) : value_float(x * y);
  case TOKEN_SLASH:
    if (y == 0)
    {
      runtime_error(line, "Divisao por zero");
      return value_null();
    }
    return value_float(x / y);
  default:
    runtime_error(line, "Operador aritmetico desconhecido");
    return value_null();
  }
}

static Value eval_comparison(Value a, Value b, TokenType op, int line)
{
  if ((a.type != VAL_INT && a.type != VAL_FLOAT) ||
      (b.type != VAL_INT && b.type != VAL_FLOAT))
  {
    runtime_error(line, "Comparacao exige operandos numericos");
    return value_bool(0);
  }
  double x = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
  double y = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
  switch (op)
  {
  case TOKEN_LESS:
    return value_bool(x < y);
  case TOKEN_LESS_EQ:
    return value_bool(x <= y);
  case TOKEN_GREATER:
    return value_bool(x > y);
  case TOKEN_GREATER_EQ:
    return value_bool(x >= y);
  default:
    return value_bool(0);
  }
}

static Value eval_binary(Expr *e, Environment *env)
{
  TokenType op = e->as.binary.op;
  if (op == TOKEN_AND)
  {
    Value left = eval_expr(e->as.binary.left, env);
    if (!value_is_truthy(left))
      return value_bool(0);
    return value_bool(value_is_truthy(eval_expr(e->as.binary.right, env)));
  }
  if (op == TOKEN_OR)
  {
    Value left = eval_expr(e->as.binary.left, env);
    if (value_is_truthy(left))
      return value_bool(1);
    return value_bool(value_is_truthy(eval_expr(e->as.binary.right, env)));
  }
  Value a = eval_expr(e->as.binary.left, env);
  Value b = eval_expr(e->as.binary.right, env);
  switch (op)
  {
  case TOKEN_PLUS:
    return eval_add(a, b, e->line);
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
    return eval_arith(a, b, op, e->line);
  case TOKEN_EQ_EQ:
    return value_bool(value_equals(a, b));
  case TOKEN_BANG_EQ:
    return value_bool(!value_equals(a, b));
  case TOKEN_LESS:
  case TOKEN_LESS_EQ:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQ:
    return eval_comparison(a, b, op, e->line);
  default:
    runtime_error(e->line, "Operador binario desconhecido");
    return value_null();
  }
}

// =================================================================
//  EXPRESSÕES — avaliador principal
// =================================================================
static Value eval_expr(Expr *expr, Environment *env)
{
  switch (expr->type)
  {
  case EXPR_INT_LITERAL:
    return value_int(expr->as.int_value);
  case EXPR_FLOAT_LITERAL:
    return value_float(expr->as.float_value);
  case EXPR_STRING_LITERAL:
    return value_string(expr->as.string_value);
  case EXPR_BOOL_LITERAL:
    return value_bool(expr->as.bool_value);

  case EXPR_IDENTIFIER:
  {
    Value v;
    if (env_get(env, expr->as.string_value, &v))
      return v;
    runtime_error(expr->line, "Variavel nao definida");
    return value_null();
  }

  case EXPR_GROUPING:
    return eval_expr(expr->as.grouping.inner, env);
  case EXPR_BINARY:
    return eval_binary(expr, env);

  case EXPR_UNARY:
  {
    Value operand = eval_expr(expr->as.unary.operand, env);
    if (expr->as.unary.op == TOKEN_MINUS)
    {
      if (operand.type == VAL_INT)
        return value_int(-operand.as.int_val);
      if (operand.type == VAL_FLOAT)
        return value_float(-operand.as.float_val);
      runtime_error(expr->line, "Operando de '-' deve ser numerico");
      return value_null();
    }
    if (expr->as.unary.op == TOKEN_BANG)
      return value_bool(!value_is_truthy(operand));
    return value_null();
  }

  case EXPR_ASSIGN:
  {
    Value val = eval_expr(expr->as.assign.value, env);
    Expr *target = expr->as.assign.target;
    if (target->type == EXPR_IDENTIFIER)
    {
      if (!env_assign(env, target->as.string_value, val))
        env_define(env, target->as.string_value, val);
      return val;
    }
    if (target->type == EXPR_GET)
    {
      Value obj = eval_expr(target->as.get.object, env);
      if (obj.type == VAL_INSTANCE)
      {
        instance_set(obj.as.instance, target->as.get.name, val);
        return val;
      }
      if (obj.type == VAL_OBJECT)
      {
        object_set(obj.as.object, target->as.get.name, val);
        return val;
      }
      runtime_error(expr->line, "Atribuicao de campo exige objeto ou instancia");
      return value_null();
    }
    runtime_error(expr->line, "Alvo de atribuicao invalido");
    return value_null();
  }

  case EXPR_POSTFIX:
  {
    Expr *target = expr->as.postfix.operand;

    // suporta this.campo++ e variavel++
    if (target->type == EXPR_GET)
    {
      Value obj = eval_expr(target->as.get.object, env);
      if (obj.type != VAL_INSTANCE && obj.type != VAL_OBJECT)
      {
        runtime_error(expr->line, "'++'/'--' exige campo de instancia/objeto");
        return value_null();
      }
      Value cur;
      int found = (obj.type == VAL_INSTANCE)
                      ? instance_get(obj.as.instance, target->as.get.name, &cur)
                      : object_get(obj.as.object, target->as.get.name, &cur);
      if (!found)
      {
        runtime_error(expr->line, "Campo nao encontrado");
        return value_null();
      }
      Value updated;
      if (cur.type == VAL_INT)
        updated = value_int(cur.as.int_val + (expr->as.postfix.op == TOKEN_PLUS_PLUS ? 1 : -1));
      else if (cur.type == VAL_FLOAT)
        updated = value_float(cur.as.float_val + (expr->as.postfix.op == TOKEN_PLUS_PLUS ? 1 : -1));
      else
      {
        runtime_error(expr->line, "'++'/'--' exige numero");
        return value_null();
      }
      if (obj.type == VAL_INSTANCE)
        instance_set(obj.as.instance, target->as.get.name, updated);
      else
        object_set(obj.as.object, target->as.get.name, updated);
      return cur;
    }

    if (target->type != EXPR_IDENTIFIER)
    {
      runtime_error(expr->line, "'++'/'--' exige uma variavel");
      return value_null();
    }
    Value cur;
    if (!env_get(env, target->as.string_value, &cur))
    {
      runtime_error(expr->line, "Variavel nao definida");
      return value_null();
    }
    Value updated;
    if (cur.type == VAL_INT)
      updated = value_int(cur.as.int_val + (expr->as.postfix.op == TOKEN_PLUS_PLUS ? 1 : -1));
    else if (cur.type == VAL_FLOAT)
      updated = value_float(cur.as.float_val + (expr->as.postfix.op == TOKEN_PLUS_PLUS ? 1 : -1));
    else
    {
      runtime_error(expr->line, "'++'/'--' exige numero");
      return value_null();
    }
    env_assign(env, target->as.string_value, updated);
    return cur;
  }

  case EXPR_ARRAY:
  {
    Value arr = value_array();
    for (int i = 0; i < expr->as.array.count; i++)
      array_push(arr.as.array, eval_expr(expr->as.array.elements[i], env));
    return arr;
  }

  case EXPR_OBJECT:
  {
    Value obj = value_object();
    for (int i = 0; i < expr->as.object.count; i++)
      object_set(obj.as.object, expr->as.object.keys[i],
                 eval_expr(expr->as.object.values[i], env));
    return obj;
  }

  case EXPR_GET:
  {
    Value obj = eval_expr(expr->as.get.object, env);
    if (obj.type == VAL_OBJECT)
    {
      Value out;
      if (object_get(obj.as.object, expr->as.get.name, &out))
        return out;
      runtime_error(expr->line, "Propriedade nao encontrada");
      return value_null();
    }
    if (obj.type == VAL_INSTANCE)
    {
      Value out;
      if (instance_get(obj.as.instance, expr->as.get.name, &out))
        return out;
      runtime_error(expr->line, "Campo nao encontrado");
      return value_null();
    }
    runtime_error(expr->line, "Acesso a propriedade exige objeto ou instancia");
    return value_null();
  }

  case EXPR_CALL:
  {
    // --- 1) Chamada de método: obj.metodo(args) ---
    if (expr->as.call.callee->type == EXPR_GET)
    {
      Expr *get = expr->as.call.callee;
      Value target = eval_expr(get->as.get.object, env);
      if (target.type == VAL_INSTANCE)
      {
        Member *m = find_method(target.as.instance->klass, get->as.get.name);
        if (m)
        {
          int n = expr->as.call.arg_count;
          Value *args = (n > 0) ? malloc(sizeof(Value) * n) : NULL;
          for (int i = 0; i < n; i++)
            args[i] = eval_expr(expr->as.call.args[i], env);
          Value r = call_method(m, interp.globals, target, args, n);
          if (args)
            free(args);
          return r;
        }
        runtime_error(expr->line, "Metodo nao encontrado");
        return value_null();
      }
      runtime_error(expr->line, "Chamada de metodo exige uma instancia");
      return value_null();
    }

    // --- 2) print nativo ---
    Expr *callee = expr->as.call.callee;
    if (callee->type == EXPR_IDENTIFIER &&
        strcmp(callee->as.string_value, "print") == 0)
    {
      for (int i = 0; i < expr->as.call.arg_count; i++)
      {
        if (i > 0)
          printf(" ");
        value_print(eval_expr(expr->as.call.args[i], env));
      }
      printf("\n");
      return value_null();
    }

    // --- 3) new Classe(...) — identificador que é uma classe ---
    if (callee->type == EXPR_IDENTIFIER)
    {
      ObjClass *klass = find_class(env, callee->as.string_value);
      if (klass)
      {
        Value inst_val = value_instance(klass);
        ObjInstance *inst = inst_val.as.instance;

        // inicializa campos da cadeia de heranca (base primeiro)
        // coletamos a cadeia e percorremos de tras pra frente
        ObjClass *chain[64];
        int depth = 0;
        for (ObjClass *k = klass; k != NULL && depth < 64; k = k->super)
          chain[depth++] = k;
        for (int c = depth - 1; c >= 0; c--)
        {
          Stmt *decl = (Stmt *)chain[c]->decl;
          for (int i = 0; i < decl->as.class_decl.member_count; i++)
          {
            Member *m = decl->as.class_decl.members[i];
            if (!m->is_method)
            {
              Value fv = m->field_init
                             ? eval_expr(m->field_init, env)
                             : value_null();
              instance_set(inst, m->name, fv);
            }
          }
        }

        // roda construtor, se existir
        Member *ctor = find_method(klass, "construct");
        if (ctor)
        {
          int n = expr->as.call.arg_count;
          Value *args = (n > 0) ? malloc(sizeof(Value) * n) : NULL;
          for (int i = 0; i < n; i++)
            args[i] = eval_expr(expr->as.call.args[i], env);
          call_method(ctor, interp.globals, inst_val, args, n);
          if (args)
            free(args);
        }
        return inst_val;
      }
    }

    // --- 4) chamada de função normal ---
    Value fn = eval_expr(callee, env);
    if (fn.type != VAL_FUNCTION)
    {
      runtime_error(expr->line, "Tentativa de chamar algo que nao e funcao");
      return value_null();
    }
    int n = expr->as.call.arg_count;
    Value *args = (n > 0) ? malloc(sizeof(Value) * n) : NULL;
    for (int i = 0; i < n; i++)
      args[i] = eval_expr(expr->as.call.args[i], env);
    Value result = call_user_function((Stmt *)fn.as.function->decl, (Environment *)fn.as.function->closure, args, n);
    if (args)
      free(args);
    return result;
  }

  default:
    runtime_error(expr->line, "Expressao nao suportada");
    return value_null();
  }
}

// =================================================================
//  STATEMENTS
// =================================================================
static void exec_block(Stmt *block, Environment *env)
{
  for (int i = 0; i < block->as.block.count; i++)
  {
    exec_stmt(block->as.block.statements[i], env);
    if (interp.returning || interp.had_error)
      return;
  }
}

static void exec_stmt(Stmt *stmt, Environment *env)
{
  switch (stmt->type)
  {
  case STMT_VAR_DECL:
  case STMT_CONST_DECL:
  {
    Value val = stmt->as.var_decl.initializer
                    ? eval_expr(stmt->as.var_decl.initializer, env)
                    : value_null();
    env_define(env, stmt->as.var_decl.name, val);
    break;
  }

  case STMT_FUNC_DECL:
  {
    Value fn = value_function(stmt, env, stmt->as.func_decl.name);
    env_define(env, stmt->as.func_decl.name, fn);
    break;
  }

  case STMT_EXPRESSION:
    eval_expr(stmt->as.expr_stmt.expression, env);
    break;

  case STMT_RETURN:
    interp.return_value = stmt->as.ret.value
                              ? eval_expr(stmt->as.ret.value, env)
                              : value_null();
    interp.returning = 1;
    break;

  case STMT_IF:
  {
    Value cond = eval_expr(stmt->as.if_stmt.condition, env);
    if (value_is_truthy(cond))
      exec_stmt(stmt->as.if_stmt.then_branch, env);
    else if (stmt->as.if_stmt.else_branch)
      exec_stmt(stmt->as.if_stmt.else_branch, env);
    break;
  }

  case STMT_WHILE:
    while (value_is_truthy(eval_expr(stmt->as.while_stmt.condition, env)))
    {
      exec_stmt(stmt->as.while_stmt.body, env);
      if (interp.returning || interp.had_error)
        return;
    }
    break;

  case STMT_FOREACH:
  {
    Value iterable = eval_expr(stmt->as.foreach_stmt.iterable, env);
    if (iterable.type != VAL_ARRAY)
    {
      runtime_error(stmt->line, "foreach exige um array");
      return;
    }
    Environment *loop_env = env_new(env);
    for (int i = 0; i < iterable.as.array->count; i++)
    {
      env_define(loop_env, stmt->as.foreach_stmt.var_name,
                 iterable.as.array->items[i]);
      exec_stmt(stmt->as.foreach_stmt.body, loop_env);
      if (interp.returning || interp.had_error)
      {
        env_free(loop_env);
        return;
      }
    }
    env_free(loop_env);
    break;
  }

  case STMT_FOR:
  {
    Value iterable = eval_expr(stmt->as.for_stmt.iterable, env);
    if (iterable.type != VAL_ARRAY)
    {
      runtime_error(stmt->line, "for-in exige um array");
      return;
    }
    Environment *loop_env = env_new(env);
    for (int i = 0; i < iterable.as.array->count; i++)
    {
      env_define(loop_env, stmt->as.for_stmt.var_name,
                 iterable.as.array->items[i]);
      exec_stmt(stmt->as.for_stmt.body, loop_env);
      if (interp.returning || interp.had_error)
      {
        env_free(loop_env);
        return;
      }
    }
    env_free(loop_env);
    break;
  }

  case STMT_BLOCK:
  {
    Environment *block_env = env_new(env);
    exec_block(stmt, block_env);
    env_free(block_env);
    break;
  }

  case STMT_CLASS:
  {
    ObjClass *super = NULL;
    if (stmt->as.class_decl.superclass)
    {
      super = find_class(env, stmt->as.class_decl.superclass);
      if (!super)
        runtime_error(stmt->line, "Superclasse nao encontrada");
    }
    Value klass = value_class(stmt->as.class_decl.name, stmt, super);
    env_define(env, stmt->as.class_decl.name, klass);
    break;
  }

  // Entram em fases seguintes
  case STMT_INTERFACE:
  case STMT_TRAIT:
  case STMT_ENUM:
  case STMT_NAMESPACE:
  case STMT_IMPORT:
    break;

  default:
    runtime_error(stmt->line, "Statement nao suportado");
    break;
  }
}

// =================================================================
//  ENTRADA
// =================================================================
int interpret(Stmt *program)
{
  interp.globals = env_new(NULL);
  interp.had_error = 0;
  interp.returning = 0;

  // 1ª passada: registra classes e funções (permite usar antes de declarar)
  for (int i = 0; i < program->as.block.count; i++)
  {
    Stmt *s = program->as.block.statements[i];
    if (s->type == STMT_CLASS || s->type == STMT_FUNC_DECL)
      exec_stmt(s, interp.globals);
  }
  // 2ª passada: executa o restante
  for (int i = 0; i < program->as.block.count; i++)
  {
    Stmt *s = program->as.block.statements[i];
    if (s->type == STMT_CLASS || s->type == STMT_FUNC_DECL)
      continue;
    exec_stmt(s, interp.globals);
    if (interp.had_error)
      break;
  }

  return interp.had_error;
}