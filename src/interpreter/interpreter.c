#include "interpreter.h"
#include "value.h"
#include "environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estado global do interpretador
typedef struct
{
  Environment *globals;
  int had_error;
  // Sinalização de return: quando set, propaga até a chamada da função
  int returning;
  Value return_value;
} Interp;

static Interp interp;

// ----- Erro de runtime -----
static void runtime_error(int line, const char *msg)
{
  fprintf(stderr, "[linha %d] Erro de execucao: %s\n", line, msg);
  interp.had_error = 1;
}

// ----- Forward declarations -----
static Value eval_expr(Expr *expr, Environment *env);
static void exec_stmt(Stmt *stmt, Environment *env);
static void exec_block(Stmt *block, Environment *env);

// =================================================================
//  EXPRESSÕES
// =================================================================

// Concatenação/soma: se algum lado for string, concatena; senão, soma numérica
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

// Operações numéricas genéricas (-, *, /)
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
    // divisão sempre retorna float para não perder precisão
    return value_float(x / y);
  default:
    runtime_error(line, "Operador aritmetico desconhecido");
    return value_null();
  }
}

static Value eval_comparison(Value a, Value b, TokenType op, int line)
{
  double x = (a.type == VAL_FLOAT) ? a.as.float_val : (a.type == VAL_INT ? (double)a.as.int_val : 0);
  double y = (b.type == VAL_FLOAT) ? b.as.float_val : (b.type == VAL_INT ? (double)b.as.int_val : 0);

  if ((a.type != VAL_INT && a.type != VAL_FLOAT) ||
      (b.type != VAL_INT && b.type != VAL_FLOAT))
  {
    runtime_error(line, "Comparacao exige operandos numericos");
    return value_bool(0);
  }
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

  // && e || com curto-circuito
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
      // se não existe, cria no escopo atual (modo dinâmico)
      if (!env_assign(env, target->as.string_value, val))
        env_define(env, target->as.string_value, val);
      return val;
    }
    runtime_error(expr->line, "Alvo de atribuicao invalido");
    return value_null();
  }

  case EXPR_POSTFIX:
  {
    // x++ / x-- : lê, incrementa, regrava; retorna valor ORIGINAL
    Expr *target = expr->as.postfix.operand;
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
    runtime_error(expr->line, "Acesso a propriedade exige um objeto");
    return value_null();
  }

  case EXPR_CALL:
  {
    // Função nativa: print
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

    // Função definida pelo usuário
    Value fn = eval_expr(callee, env);
    if (fn.type != VAL_FUNCTION)
    {
      runtime_error(expr->line, "Tentativa de chamar algo que nao e funcao");
      return value_null();
    }
    ObjFunction *func = fn.as.function;
    Stmt *decl = (Stmt *)func->decl;
    Environment *closure = (Environment *)func->closure;

    // Novo escopo para a chamada
    Environment *call_env = env_new(closure);

    int param_count = decl->as.func_decl.param_count;
    for (int i = 0; i < param_count; i++)
    {
      Value arg = (i < expr->as.call.arg_count)
                      ? eval_expr(expr->as.call.args[i], env)
                      : value_null();
      env_define(call_env, decl->as.func_decl.params[i].name, arg);
    }

    exec_block(decl->as.func_decl.body, call_env);

    Value result = value_null();
    if (interp.returning)
    {
      result = interp.return_value;
      interp.returning = 0; // consome o sinal de return
    }
    env_free(call_env);
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
      return; // para o bloco
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
    // Cria o valor-função capturando o escopo atual (closure)
    Value fn = value_function(stmt, env, stmt->as.func_decl.name);
    env_define(env, stmt->as.func_decl.name, fn);
    break;
  }

  case STMT_EXPRESSION:
    eval_expr(stmt->as.expr_stmt.expression, env);
    break;

  case STMT_RETURN:
  {
    interp.return_value = stmt->as.ret.value
                              ? eval_expr(stmt->as.ret.value, env)
                              : value_null();
    interp.returning = 1;
    break;
  }

  case STMT_IF:
  {
    Value cond = eval_expr(stmt->as.if_stmt.condition, env);
    if (value_is_truthy(cond))
    {
      exec_stmt(stmt->as.if_stmt.then_branch, env);
    }
    else if (stmt->as.if_stmt.else_branch)
    {
      exec_stmt(stmt->as.if_stmt.else_branch, env);
    }
    break;
  }

  case STMT_WHILE:
  {
    while (value_is_truthy(eval_expr(stmt->as.while_stmt.condition, env)))
    {
      exec_stmt(stmt->as.while_stmt.body, env);
      if (interp.returning || interp.had_error)
        return;
    }
    break;
  }

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
    // for x in <array>  — percorre os elementos
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

  // Declarações de OOP/módulos: ignoradas nesta fase do interpretador
  case STMT_CLASS:
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

  // Executa cada statement do programa no escopo global
  for (int i = 0; i < program->as.block.count; i++)
  {
    exec_stmt(program->as.block.statements[i], interp.globals);
    if (interp.had_error)
      break;
  }

  env_free(interp.globals);
  return interp.had_error;
}