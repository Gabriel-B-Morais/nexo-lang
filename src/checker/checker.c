#include "checker.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Tipos que o checker entende internamente
typedef enum
{
  T_INT,
  T_FLOAT,
  T_STRING,
  T_BOOL,
  T_OBJECT,
  T_ARRAY,
  T_NULL,
  T_ANY,    // tipo desconhecido/dinâmico — aceita qualquer coisa
  T_UNKNOWN // erro ou não inferível
} CType;

// Tabela de tipos de variáveis (com escopos aninhados)
typedef struct Scope
{
  char names[256][64]; // nomes das variáveis (até 256 por escopo)
  CType types[256];
  int count;
  struct Scope *parent;
} Scope;

// Estado do checker
typedef struct
{
  int strict;
  int errors;
  Scope *scope;
} Checker;

// Converte CType em texto para mensagens de erro
static const char *ctype_name(CType t)
{
  switch (t)
  {
  case T_INT:
    return "int";
  case T_FLOAT:
    return "float";
  case T_STRING:
    return "string";
  case T_BOOL:
    return "bool";
  case T_OBJECT:
    return "object";
  case T_ARRAY:
    return "array";
  case T_NULL:
    return "null";
  case T_ANY:
    return "any";
  default:
    return "unknown";
  }
}

// Reporta um erro de tipo
static void type_error(Checker *c, int line, const char *msg)
{
  fprintf(stderr, "[linha %d] Erro de tipo: %s\n", line, msg);
  c->errors++;
}

// Cria um novo escopo filho
static Scope *scope_new(Scope *parent)
{
  Scope *s = (Scope *)malloc(sizeof(Scope));
  s->count = 0;
  s->parent = parent;
  return s;
}

// Define uma variável no escopo atual
static void scope_define(Scope *s, const char *name, CType type)
{
  // se já existe no escopo atual, atualiza
  for (int i = 0; i < s->count; i++)
  {
    if (strcmp(s->names[i], name) == 0)
    {
      s->types[i] = type;
      return;
    }
  }
  if (s->count < 256)
  {
    strncpy(s->names[s->count], name, 63);
    s->names[s->count][63] = '\0';
    s->types[s->count] = type;
    s->count++;
  }
}

// Busca o tipo de uma variável (sobe pelos escopos). Retorna 1 se achou.
static int scope_lookup(Scope *s, const char *name, CType *out)
{
  for (Scope *cur = s; cur != NULL; cur = cur->parent)
  {
    for (int i = 0; i < cur->count; i++)
    {
      if (strcmp(cur->names[i], name) == 0)
      {
        *out = cur->types[i];
        return 1;
      }
    }
  }
  return 0;
}

static int types_compatible(CType declared, CType value);
static void check_expr(Checker *c, Expr *expr);

// Deduz o tipo de uma expressão
static CType infer_type(Checker *c, Expr *expr)
{
  switch (expr->type)
  {
  case EXPR_INT_LITERAL:
    return T_INT;
  case EXPR_FLOAT_LITERAL:
    return T_FLOAT;
  case EXPR_STRING_LITERAL:
    return T_STRING;
  case EXPR_BOOL_LITERAL:
    return T_BOOL;
  case EXPR_ARRAY:
    return T_ARRAY;
  case EXPR_OBJECT:
    return T_OBJECT;
  case EXPR_GROUPING:
    return infer_type(c, expr->as.grouping.inner);
  case EXPR_UNARY:
  {
    CType operand = infer_type(c, expr->as.unary.operand);
    if (expr->as.unary.op == TOKEN_BANG)
      return T_BOOL;
    // '-' mantém o tipo numérico
    return operand;
  }
  case EXPR_BINARY:
  {
    TokenType op = expr->as.binary.op;
    CType left = infer_type(c, expr->as.binary.left);
    CType right = infer_type(c, expr->as.binary.right);
    // operadores de comparação e lógicos sempre dão bool
    if (op == TOKEN_EQ_EQ || op == TOKEN_BANG_EQ ||
        op == TOKEN_LESS || op == TOKEN_LESS_EQ ||
        op == TOKEN_GREATER || op == TOKEN_GREATER_EQ ||
        op == TOKEN_AND || op == TOKEN_OR)
    {
      return T_BOOL;
    }
    // '+' com string de um lado vira concatenação (string)
    if (op == TOKEN_PLUS && (left == T_STRING || right == T_STRING))
    {
      return T_STRING;
    }
    // aritmética: se algum é float, resultado é float
    if (left == T_FLOAT || right == T_FLOAT)
      return T_FLOAT;
    if (left == T_INT && right == T_INT)
    {
      // divisão sempre retorna float (como definimos no interpretador)
      if (op == TOKEN_SLASH)
        return T_FLOAT;
      return T_INT;
    }
    return T_ANY;
  }
  case EXPR_IDENTIFIER:
  {
    CType t;
    if (scope_lookup(c->scope, expr->as.string_value, &t))
      return t;
    return T_ANY; // variável desconhecida — tratamos como any
  }
    return T_ANY;
  case EXPR_CALL:
    return T_ANY;
  case EXPR_GET:
    return T_ANY;
  case EXPR_ASSIGN:
    return infer_type(c, expr->as.assign.value);
  case EXPR_POSTFIX:
    return T_ANY;
  default:
    return T_ANY;
  }
}

// Converte um Type da AST (o que foi escrito) em CType (o que o checker usa)
static CType type_to_ctype(Type *t)
{
  if (!t)
    return T_ANY; // sem tipo declarado
  if (t->kind == TYPE_SIMPLE)
  {
    const char *n = t->as.name;
    if (strcmp(n, "int") == 0)
      return T_INT;
    if (strcmp(n, "float") == 0)
      return T_FLOAT;
    if (strcmp(n, "string") == 0)
      return T_STRING;
    if (strcmp(n, "bool") == 0)
      return T_BOOL;
    if (strcmp(n, "object") == 0)
      return T_OBJECT;
    if (strcmp(n, "numeric") == 0)
      return T_FLOAT; // numeric aceita int/float
    return T_ANY;     // tipo de classe (Pessoa, User...) — tratamos como any por ora
  }
  if (t->kind == TYPE_GENERIC)
    return T_ARRAY; // array<...>
  if (t->kind == TYPE_UNION)
    return T_ANY; // union — refinamos depois
  return T_ANY;
}

// Percorre e valida um statement
static void check_stmt(Checker *c, Stmt *stmt)
{
  switch (stmt->type)
  {
  case STMT_VAR_DECL:
  case STMT_CONST_DECL:
  {
    Type *declared = stmt->as.var_decl.var_type;

    // Modo strict: variável precisa de tipo declarado
    if (c->strict && !declared)
    {
      if (!stmt->as.var_decl.initializer)
      {
        type_error(c, stmt->line,
                   "modo strict exige tipo declarado ou valor inicial");
      }
    }

    // Se tem tipo declarado E valor, checa compatibilidade (Opção B)
    if (declared && stmt->as.var_decl.initializer)
    {
      CType dt = type_to_ctype(declared);
      CType vt = infer_type(c, stmt->as.var_decl.initializer);
      if (!types_compatible(dt, vt))
      {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "nao e possivel atribuir %s a variavel do tipo %s",
                 ctype_name(vt), ctype_name(dt));
        type_error(c, stmt->line, msg);
      }
    }

    // Registra a variável na tabela de tipos
    CType var_type;
    if (declared)
    {
      var_type = type_to_ctype(declared);
    }
    else if (stmt->as.var_decl.initializer)
    {
      var_type = infer_type(c, stmt->as.var_decl.initializer);
    }
    else
    {
      var_type = T_ANY;
    }

    // Em modo dinâmico, variável sem tipo declarado fica livre (T_ANY).
    // Em strict, ela trava no tipo inferido acima.
    if (!c->strict && !declared)
    {
      var_type = T_ANY;
    }

    scope_define(c->scope, stmt->as.var_decl.name, var_type);

    break;
  }

  case STMT_BLOCK:
    for (int i = 0; i < stmt->as.block.count; i++)
      check_stmt(c, stmt->as.block.statements[i]);
    break;

  case STMT_IF:
    check_stmt(c, stmt->as.if_stmt.then_branch);
    if (stmt->as.if_stmt.else_branch)
      check_stmt(c, stmt->as.if_stmt.else_branch);
    break;

  case STMT_WHILE:
    check_stmt(c, stmt->as.while_stmt.body);
    break;

  case STMT_FOREACH:
    check_stmt(c, stmt->as.foreach_stmt.body);
    break;

  case STMT_FOR:
    check_stmt(c, stmt->as.for_stmt.body);
    break;

  case STMT_FUNC_DECL:
    check_stmt(c, stmt->as.func_decl.body);
    break;
  case STMT_EXPRESSION:
    check_expr(c, stmt->as.expr_stmt.expression);
    break;
  // outros statements: ainda não validamos
  default:
    break;
  }
}

// Percorre uma expressão validando regras (atribuições, etc.)
static void check_expr(Checker *c, Expr *expr)
{
  if (!expr)
    return;

  switch (expr->type)
  {
  case EXPR_ASSIGN:
  {
    // valida o lado direito primeiro
    check_expr(c, expr->as.assign.value);

    Expr *target = expr->as.assign.target;
    if (target->type == EXPR_IDENTIFIER)
    {
      CType existing;
      if (scope_lookup(c->scope, target->as.string_value, &existing))
      {
        CType new_type = infer_type(c, expr->as.assign.value);
        // se a variável tem tipo conhecido (não any), o novo valor deve bater
        if (existing != T_ANY &&
            !types_compatible(existing, new_type))
        {
          char msg[160];
          snprintf(msg, sizeof(msg),
                   "nao e possivel atribuir %s a '%s' (do tipo %s)",
                   ctype_name(new_type), target->as.string_value,
                   ctype_name(existing));
          type_error(c, expr->line, msg);
        }
      }
    }
    break;
  }

  case EXPR_BINARY:
    check_expr(c, expr->as.binary.left);
    check_expr(c, expr->as.binary.right);
    break;

  case EXPR_UNARY:
    check_expr(c, expr->as.unary.operand);
    break;

  case EXPR_GROUPING:
    check_expr(c, expr->as.grouping.inner);
    break;

  case EXPR_CALL:
    check_expr(c, expr->as.call.callee);
    for (int i = 0; i < expr->as.call.arg_count; i++)
      check_expr(c, expr->as.call.args[i]);
    break;

  default:
    break;
  }
}

// Verifica se o tipo do valor é compatível com o tipo declarado
static int types_compatible(CType declared, CType value)
{
  if (declared == T_ANY || value == T_ANY)
    return 1; // any aceita tudo
  if (declared == value)
    return 1;
  // int pode ir para float (promoção natural)
  if (declared == T_FLOAT && value == T_INT)
    return 1;
  return 0;
}

// Detecta '#!strict' como primeira INSTRUÇÃO do arquivo.
// Comentários (//) e linhas em branco antes são ignorados.
int detect_strict_mode(const char *source)
{
  const char *p = source;
  for (;;)
  {
    // pula espaços e quebras de linha
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      p++;

    // pula comentário de linha
    if (p[0] == '/' && p[1] == '/')
    {
      while (*p != '\n' && *p != '\0')
        p++;
      continue; // volta a pular espaços/linhas
    }

    // chegou no primeiro conteúdo real
    break;
  }
  return strncmp(p, "#!strict", 8) == 0;
}

int check_types(Stmt *program, int strict_mode)
{
  Checker c;
  c.strict = strict_mode;
  c.errors = 0;
  c.scope = scope_new(NULL);

  for (int i = 0; i < program->as.block.count; i++)
  {
    check_stmt(&c, program->as.block.statements[i]);
  }

  return c.errors;
}