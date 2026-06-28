#include "parser.h"
#include "../lexer/lexer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct
{
  Lexer *lexer;
  Token previous;
  Token current;
  Token next; // 1 token de lookahead
  int had_error;
  int panic_mode;
} Parser;

// ----- Forward declarations (são mutuamente recursivas) -----
static Expr *expression(Parser *p);
static Expr *assignment(Parser *p);
static Expr *or_expr(Parser *p);
static Expr *and_expr(Parser *p);
static Expr *equality(Parser *p);
static Expr *comparison(Parser *p);
static Expr *term(Parser *p);
static Expr *factor(Parser *p);
static Expr *unary(Parser *p);
static Expr *postfix(Parser *p);
static Expr *primary(Parser *p);
static Expr *finish_call(Parser *p, Expr *callee);
static Expr *array_literal(Parser *p, int line);
static Expr *object_literal(Parser *p, int line);

static Type *parse_type(Parser *p);
static Type *parse_base_type(Parser *p);

static Stmt *declaration(Parser *p);
static Stmt *statement(Parser *p);
static Stmt *var_declaration(Parser *p);
static Stmt *const_declaration(Parser *p);
static Stmt *func_declaration(Parser *p);
static Stmt *return_statement(Parser *p);
static Stmt *if_statement(Parser *p);
static Stmt *while_statement(Parser *p);
static Stmt *foreach_statement(Parser *p);
static Stmt *for_statement(Parser *p);
static Stmt *class_declaration(Parser *p);
static Stmt *interface_declaration(Parser *p);
static Stmt *trait_declaration(Parser *p);
static Stmt *enum_declaration(Parser *p);
static Stmt *namespace_declaration(Parser *p);
static Stmt *import_declaration(Parser *p);
static int parse_modifiers(Parser *p);
static Member *parse_member(Parser *p, int allow_body);
static Stmt *parse_block_body(Parser *p);
static Param parse_param(Parser *p);
static void synchronize(Parser *p);

// ----- Construtores de nós -----
static Expr *new_expr(ExprType type, int line)
{
  Expr *e = (Expr *)malloc(sizeof(Expr));
  e->type = type;
  e->line = line;
  return e;
}
static Stmt *new_stmt(StmtType type, int line)
{
  Stmt *s = (Stmt *)malloc(sizeof(Stmt));
  s->type = type;
  s->line = line;
  return s;
}
static Type *new_type(TypeKind kind)
{
  Type *t = (Type *)malloc(sizeof(Type));
  t->kind = kind;
  return t;
}
static Expr *new_binary(Expr *left, TokenType op, Expr *right, int line)
{
  Expr *e = new_expr(EXPR_BINARY, line);
  e->as.binary.left = left;
  e->as.binary.op = op;
  e->as.binary.right = right;
  return e;
}

// ----- Helpers de token -----
static void advance_parser(Parser *p)
{
  p->previous = p->current;
  p->current = p->next;
  p->next = lexer_next_token(p->lexer);
}
static int check(Parser *p, TokenType type) { return p->current.type == type; }
static int check_next(Parser *p, TokenType type) { return p->next.type == type; }
static int match_tok(Parser *p, TokenType type)
{
  if (!check(p, type))
    return 0;
  advance_parser(p);
  return 1;
}
static void skip_newlines(Parser *p)
{
  while (check(p, TOKEN_NEWLINE))
    advance_parser(p);
}
static void error_at(Parser *p, Token *token, const char *message)
{
  if (p->panic_mode)
    return;
  p->panic_mode = 1;
  p->had_error = 1;
  fprintf(stderr, "[linha %d] Erro de sintaxe", token->line);
  if (token->type == TOKEN_EOF)
    fprintf(stderr, " no fim do arquivo");
  else if (token->type == TOKEN_NEWLINE)
    fprintf(stderr, " no fim da linha");
  else
    fprintf(stderr, " perto de '%s'", token->lexeme);
  fprintf(stderr, ": %s\n", message);
}
static void error_at_current(Parser *p, const char *message)
{
  error_at(p, &p->current, message);
}
static void consume(Parser *p, TokenType type, const char *message)
{
  if (check(p, type))
  {
    advance_parser(p);
    return;
  }
  error_at_current(p, message);
}

// Remove as aspas de uma string literal ("Gabriel" -> Gabriel)
static char *strip_quotes(const char *s)
{
  size_t len = strlen(s);
  if (len >= 2)
  {
    char *out = (char *)malloc(len - 1);
    memcpy(out, s + 1, len - 2);
    out[len - 2] = '\0';
    return out;
  }
  return strdup("");
}
static int is_type_token(TokenType t)
{
  switch (t)
  {
  case TOKEN_TYPE_INT:
  case TOKEN_TYPE_FLOAT:
  case TOKEN_TYPE_STRING:
  case TOKEN_TYPE_BOOL:
  case TOKEN_TYPE_OBJECT:
  case TOKEN_TYPE_ARRAY:
  case TOKEN_TYPE_NUMERIC:
  case TOKEN_IDENTIFIER:
    return 1;
  default:
    return 0;
  }
}
// =================================================================

//  TIPOS

// =================================================================

static Type *parse_base_type(Parser *p)
{

  // Tipo nomeado (primitivo ou classe) com generics opcionais: nome<...>

  char *name = strdup(p->current.lexeme);

  int is_array = check(p, TOKEN_TYPE_ARRAY);

  advance_parser(p);
  if (check(p, TOKEN_LESS))
  { // generic: array<int>, Collection<T>
    advance_parser(p);
    Type *t = new_type(TYPE_GENERIC);
    t->as.generic.name = name;
    t->as.generic.args = NULL;
    t->as.generic.arg_count = 0;

    int capacity = 0;
    do
    {
      Type *arg = parse_type(p);
      if (t->as.generic.arg_count + 1 > capacity)
      {
        capacity = capacity < 4 ? 4 : capacity * 2;
        t->as.generic.args = realloc(t->as.generic.args, sizeof(Type *) * capacity);
      }
      t->as.generic.args[t->as.generic.arg_count++] = arg;
    } while (match_tok(p, TOKEN_COMMA));

    consume(p, TOKEN_GREATER, "Esperado '>' para fechar o tipo generico");
    (void)is_array;
    return t;
  }

  Type *t = new_type(TYPE_SIMPLE);
  t->as.name = name;
  return t;
}
static Type *parse_type(Parser *p)
{

  Type *first = parse_base_type(p);

  if (!check(p, TOKEN_PIPE))
    return first;
  // Union: a | b | c
  Type *t = new_type(TYPE_UNION);
  t->as.union_type.members = NULL;
  t->as.union_type.count = 0;
  int capacity = 0;

#define PUSH_MEMBER(m)                                                                         \
  do                                                                                           \
  {                                                                                            \
    if (t->as.union_type.count + 1 > capacity)                                                 \
    {                                                                                          \
      capacity = capacity < 4 ? 4 : capacity * 2;                                              \
      t->as.union_type.members = realloc(t->as.union_type.members, sizeof(Type *) * capacity); \
    }                                                                                          \
    t->as.union_type.members[t->as.union_type.count++] = (m);                                  \
  } while (0)

  PUSH_MEMBER(first);
  while (match_tok(p, TOKEN_PIPE))
    PUSH_MEMBER(parse_base_type(p));
#undef PUSH_MEMBER
  return t;
}
// =================================================================

//  EXPRESSÕES  (menor precedência -> maior precedência)

// =================================================================

static Expr *expression(Parser *p) { return assignment(p); }
static Expr *assignment(Parser *p)
{

  Expr *left = or_expr(p);

  if (check(p, TOKEN_EQUALS))
  {

    int line = p->current.line;

    advance_parser(p);

    Expr *value = assignment(p); // associatividade à direita

    if (left->type == EXPR_IDENTIFIER || left->type == EXPR_GET)
    {

      Expr *e = new_expr(EXPR_ASSIGN, line);

      e->as.assign.target = left;

      e->as.assign.value = value;

      return e;
    }

    error_at_current(p, "Alvo de atribuicao invalido");
  }

  return left;
}
static Expr *or_expr(Parser *p)
{

  Expr *expr = and_expr(p);

  while (check(p, TOKEN_OR))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    expr = new_binary(expr, op, and_expr(p), line);
  }

  return expr;
}

static Expr *and_expr(Parser *p)
{

  Expr *expr = equality(p);

  while (check(p, TOKEN_AND))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    expr = new_binary(expr, op, equality(p), line);
  }

  return expr;
}

static Expr *equality(Parser *p)
{

  Expr *expr = comparison(p);

  while (check(p, TOKEN_EQ_EQ) || check(p, TOKEN_BANG_EQ))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    expr = new_binary(expr, op, comparison(p), line);
  }

  return expr;
}

static Expr *comparison(Parser *p)
{

  Expr *expr = term(p);

  while (check(p, TOKEN_LESS) || check(p, TOKEN_LESS_EQ) ||

         check(p, TOKEN_GREATER) || check(p, TOKEN_GREATER_EQ))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    expr = new_binary(expr, op, term(p), line);
  }

  return expr;
}

static Expr *term(Parser *p)
{

  Expr *expr = factor(p);

  while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    expr = new_binary(expr, op, factor(p), line);
  }

  return expr;
}

static Expr *factor(Parser *p)
{

  Expr *expr = unary(p);

  while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    expr = new_binary(expr, op, unary(p), line);
  }

  return expr;
}

static Expr *unary(Parser *p)
{

  if (check(p, TOKEN_BANG) || check(p, TOKEN_MINUS))
  {

    TokenType op = p->current.type;
    int line = p->current.line;

    advance_parser(p);

    Expr *e = new_expr(EXPR_UNARY, line);

    e->as.unary.op = op;

    e->as.unary.operand = unary(p);

    return e;
  }

  return postfix(p);
}

// pós-fixo (++ --) e encadeamento (.prop e chamadas)

static Expr *postfix(Parser *p)
{

  Expr *expr = primary(p);

  for (;;)
  {

    if (check(p, TOKEN_DOT))
    {

      advance_parser(p);

      consume(p, TOKEN_IDENTIFIER, "Esperado nome da propriedade apos '.'");

      Expr *get = new_expr(EXPR_GET, p->previous.line);

      get->as.get.object = expr;

      get->as.get.name = strdup(p->previous.lexeme);

      expr = get;
    }
    else if (check(p, TOKEN_LPAREN))
    {

      advance_parser(p);

      expr = finish_call(p, expr);
    }
    else if (check(p, TOKEN_PLUS_PLUS) || check(p, TOKEN_MINUS_MINUS))
    {

      TokenType op = p->current.type;
      int line = p->current.line;

      advance_parser(p);

      Expr *e = new_expr(EXPR_POSTFIX, line);

      e->as.postfix.operand = expr;

      e->as.postfix.op = op;

      expr = e;
    }
    else
    {

      break;
    }
  }

  return expr;
}

static Expr *finish_call(Parser *p, Expr *callee)
{

  Expr *e = new_expr(EXPR_CALL, p->previous.line);

  e->as.call.callee = callee;

  e->as.call.args = NULL;

  e->as.call.arg_count = 0;

  int capacity = 0;
  skip_newlines(p);
  if (!check(p, TOKEN_RPAREN))
  {
    do
    {
      skip_newlines(p);
      Expr *arg = expression(p);
      if (e->as.call.arg_count + 1 > capacity)
      {
        capacity = capacity < 4 ? 4 : capacity * 2;
        e->as.call.args = realloc(e->as.call.args, sizeof(Expr *) * capacity);
      }
      e->as.call.args[e->as.call.arg_count++] = arg;
      skip_newlines(p);
    } while (match_tok(p, TOKEN_COMMA));
  }
  skip_newlines(p);
  consume(p, TOKEN_RPAREN, "Esperado ')' apos os argumentos");
  return e;
}
static Expr *primary(Parser *p)
{

  int line = p->current.line;
  if (check(p, TOKEN_INT))
  {
    Expr *e = new_expr(EXPR_INT_LITERAL, line);
    e->as.int_value = strtoll(p->current.lexeme, NULL, 10);
    advance_parser(p);
    return e;
  }
  if (check(p, TOKEN_FLOAT))
  {
    Expr *e = new_expr(EXPR_FLOAT_LITERAL, line);
    e->as.float_value = strtod(p->current.lexeme, NULL);
    advance_parser(p);
    return e;
  }
  if (check(p, TOKEN_STRING))
  {
    Expr *e = new_expr(EXPR_STRING_LITERAL, line);
    e->as.string_value = strip_quotes(p->current.lexeme);
    advance_parser(p);
    return e;
  }
  if (check(p, TOKEN_TRUE) || check(p, TOKEN_FALSE))
  {
    Expr *e = new_expr(EXPR_BOOL_LITERAL, line);
    e->as.bool_value = check(p, TOKEN_TRUE) ? 1 : 0;
    advance_parser(p);
    return e;
  }
  if (check(p, TOKEN_IDENTIFIER) || check(p, TOKEN_THIS))
  {
    Expr *e = new_expr(EXPR_IDENTIFIER, line);
    e->as.string_value = strdup(p->current.lexeme);
    advance_parser(p);
    return e;
  }
  if (check(p, TOKEN_NEW))
  {
    // new Pessoa(...)  -> tratamos como chamada ao identificador Pessoa
    advance_parser(p);
    consume(p, TOKEN_IDENTIFIER, "Esperado nome da classe apos 'new'");
    Expr *callee = new_expr(EXPR_IDENTIFIER, line);
    callee->as.string_value = strdup(p->previous.lexeme);
    if (match_tok(p, TOKEN_LPAREN))
      return finish_call(p, callee);
    return callee;
  }
  if (check(p, TOKEN_LPAREN))
  {
    advance_parser(p);
    Expr *inner = expression(p);
    consume(p, TOKEN_RPAREN, "Esperado ')' apos a expressao");
    Expr *e = new_expr(EXPR_GROUPING, line);
    e->as.grouping.inner = inner;
    return e;
  }
  if (check(p, TOKEN_LBRACKET))
  {
    advance_parser(p);
    return array_literal(p, line);
  }
  if (check(p, TOKEN_LBRACE))
  {
    advance_parser(p);
    return object_literal(p, line);
  }

  error_at_current(p, "Esperado uma expressao");
  advance_parser(p);
  Expr *e = new_expr(EXPR_INT_LITERAL, line); // nó dummy para nao quebrar
  e->as.int_value = 0;
  return e;
}
static Expr *array_literal(Parser *p, int line)
{

  Expr *e = new_expr(EXPR_ARRAY, line);

  e->as.array.elements = NULL;

  e->as.array.count = 0;

  int capacity = 0;
  skip_newlines(p);
  if (!check(p, TOKEN_RBRACKET))
  {
    do
    {
      skip_newlines(p);
      if (check(p, TOKEN_RBRACKET))
        break; // vírgula final
      Expr *el = expression(p);
      if (e->as.array.count + 1 > capacity)
      {
        capacity = capacity < 4 ? 4 : capacity * 2;
        e->as.array.elements = realloc(e->as.array.elements, sizeof(Expr *) * capacity);
      }
      e->as.array.elements[e->as.array.count++] = el;
      skip_newlines(p);
    } while (match_tok(p, TOKEN_COMMA));
  }
  skip_newlines(p);
  consume(p, TOKEN_RBRACKET, "Esperado ']' para fechar o array");
  return e;
}
static Expr *object_literal(Parser *p, int line)
{

  Expr *e = new_expr(EXPR_OBJECT, line);

  e->as.object.keys = NULL;

  e->as.object.values = NULL;

  e->as.object.count = 0;

  int capacity = 0;
  skip_newlines(p);
  if (!check(p, TOKEN_RBRACE))
  {
    do
    {
      skip_newlines(p);
      if (check(p, TOKEN_RBRACE))
        break;
      // chave: identificador ou string
      char *key;
      if (check(p, TOKEN_IDENTIFIER))
        key = strdup(p->current.lexeme);
      else if (check(p, TOKEN_STRING))
        key = strip_quotes(p->current.lexeme);
      else
      {
        error_at_current(p, "Esperado nome da chave");
        key = strdup("?");
      }
      advance_parser(p);
      consume(p, TOKEN_COLON, "Esperado ':' apos a chave");
      Expr *val = expression(p);

      if (e->as.object.count + 1 > capacity)
      {
        capacity = capacity < 4 ? 4 : capacity * 2;
        e->as.object.keys = realloc(e->as.object.keys, sizeof(char *) * capacity);
        e->as.object.values = realloc(e->as.object.values, sizeof(Expr *) * capacity);
      }
      e->as.object.keys[e->as.object.count] = key;
      e->as.object.values[e->as.object.count] = val;
      e->as.object.count++;
      skip_newlines(p);
    } while (match_tok(p, TOKEN_COMMA));
  }
  skip_newlines(p);
  consume(p, TOKEN_RBRACE, "Esperado '}' para fechar o objeto");
  return e;
}
// =================================================================

//  STATEMENTS

// =================================================================

static Param parse_param(Parser *p)
{

  Param param;
  param.type = NULL;

  consume(p, TOKEN_IDENTIFIER, "Esperado nome do parametro");

  param.name = strdup(p->previous.lexeme);

  if (match_tok(p, TOKEN_COLON))
    param.type = parse_type(p);

  return param;
}
// Lê statements até encontrar 'end' (ou EOF). Consome o 'end'.

static Stmt *parse_block_body(Parser *p)
{

  Stmt *block = new_stmt(STMT_BLOCK, p->current.line);

  block->as.block.statements = NULL;

  block->as.block.count = 0;

  int capacity = 0;
  skip_newlines(p);
  while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF))
  {
    Stmt *s = declaration(p);
    if (s)
    {
      if (block->as.block.count + 1 > capacity)
      {
        capacity = capacity < 8 ? 8 : capacity * 2;
        block->as.block.statements =
            realloc(block->as.block.statements, sizeof(Stmt *) * capacity);
      }
      block->as.block.statements[block->as.block.count++] = s;
    }
    skip_newlines(p);
  }
  consume(p, TOKEN_END, "Esperado 'end' para fechar o bloco");
  return block;
}

static Stmt *declaration(Parser *p)
{
  if (p->panic_mode)
    synchronize(p);

  if (check(p, TOKEN_NAMESPACE))
    return namespace_declaration(p);
  if (check(p, TOKEN_IMPORT))
    return import_declaration(p);
  if (check(p, TOKEN_CLASS))
    return class_declaration(p);
  if (check(p, TOKEN_INTERFACE))
    return interface_declaration(p);
  if (check(p, TOKEN_TRAIT))
    return trait_declaration(p);
  if (check(p, TOKEN_ENUM))
    return enum_declaration(p);

  // Declaração tipada: IDENT ':' ...
  if (check(p, TOKEN_IDENTIFIER) && check_next(p, TOKEN_COLON))
  {
    return var_declaration(p);
  }
  if (check(p, TOKEN_CONST))
    return const_declaration(p);
  if (check(p, TOKEN_FUNC))
    return func_declaration(p);
  return statement(p);
}

static Stmt *var_declaration(Parser *p)
{

  int line = p->current.line;

  consume(p, TOKEN_IDENTIFIER, "Esperado nome da variavel");

  char *name = strdup(p->previous.lexeme);
  Type *type = NULL;
  if (match_tok(p, TOKEN_COLON))
    type = parse_type(p);

  Expr *init = NULL;
  if (match_tok(p, TOKEN_EQUALS))
    init = expression(p);

  Stmt *s = new_stmt(STMT_VAR_DECL, line);
  s->as.var_decl.name = name;
  s->as.var_decl.var_type = type;
  s->as.var_decl.initializer = init;
  return s;
}
static Stmt *const_declaration(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'const'

  consume(p, TOKEN_IDENTIFIER, "Esperado nome da constante");

  char *name = strdup(p->previous.lexeme);
  Type *type = NULL;
  if (match_tok(p, TOKEN_COLON))
    type = parse_type(p);
  consume(p, TOKEN_EQUALS, "Constante precisa de um valor inicial");
  Expr *init = expression(p);

  Stmt *s = new_stmt(STMT_CONST_DECL, line);
  s->as.var_decl.name = name;
  s->as.var_decl.var_type = type;
  s->as.var_decl.initializer = init;
  return s;
}
static Stmt *func_declaration(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'func'

  consume(p, TOKEN_IDENTIFIER, "Esperado nome da funcao");

  char *name = strdup(p->previous.lexeme);
  Param *params = NULL;
  int param_count = 0, capacity = 0;

  // Parênteses são OPCIONAIS na declaração (conforme a doc da Nexo)
  int has_paren = match_tok(p, TOKEN_LPAREN);
  int has_params_without_paren =
      !has_paren && check(p, TOKEN_IDENTIFIER) && check_next(p, TOKEN_COLON);

  if (has_paren || has_params_without_paren)
  {
    if (!(has_paren && check(p, TOKEN_RPAREN)))
    { // permite '()' vazio
      do
      {
        Param param = parse_param(p);
        if (param_count + 1 > capacity)
        {
          capacity = capacity < 4 ? 4 : capacity * 2;
          params = realloc(params, sizeof(Param) * capacity);
        }
        params[param_count++] = param;
      } while (match_tok(p, TOKEN_COMMA));
    }
    if (has_paren)
      consume(p, TOKEN_RPAREN, "Esperado ')' apos os parametros");
  }

  Type *return_type = NULL;
  if (match_tok(p, TOKEN_ARROW))
    return_type = parse_type(p);

  skip_newlines(p);
  Stmt *body = parse_block_body(p);

  Stmt *s = new_stmt(STMT_FUNC_DECL, line);
  s->as.func_decl.name = name;
  s->as.func_decl.params = params;
  s->as.func_decl.param_count = param_count;
  s->as.func_decl.return_type = return_type;
  s->as.func_decl.body = body;
  return s;
}
static Stmt *return_statement(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'return'

  Expr *value = NULL;

  if (!check(p, TOKEN_NEWLINE) && !check(p, TOKEN_END) && !check(p, TOKEN_EOF))
  {

    value = expression(p);
  }

  Stmt *s = new_stmt(STMT_RETURN, line);

  s->as.ret.value = value;

  return s;
}
// 'if' com bloco (até end/else) OU de linha única (1 só comando, sem end)

static Stmt *if_statement(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'if'

  Expr *condition = expression(p);
  Stmt *then_branch;
  Stmt *else_branch = NULL;

  if (check(p, TOKEN_NEWLINE))
  {
    // forma em bloco
    skip_newlines(p);
    Stmt *block = new_stmt(STMT_BLOCK, line);
    block->as.block.statements = NULL;
    block->as.block.count = 0;
    int capacity = 0;
    while (!check(p, TOKEN_END) && !check(p, TOKEN_ELSE) && !check(p, TOKEN_EOF))
    {
      Stmt *s = declaration(p);
      if (s)
      {
        if (block->as.block.count + 1 > capacity)
        {
          capacity = capacity < 8 ? 8 : capacity * 2;
          block->as.block.statements =
              realloc(block->as.block.statements, sizeof(Stmt *) * capacity);
        }
        block->as.block.statements[block->as.block.count++] = s;
      }
      skip_newlines(p);
    }
    then_branch = block;

    if (match_tok(p, TOKEN_ELSE))
    {
      if (check(p, TOKEN_IF))
      {
        else_branch = if_statement(p); // else if encadeado
      }
      else
      {
        skip_newlines(p);
        else_branch = parse_block_body(p); // consome o end final
      }
    }
    else
    {
      consume(p, TOKEN_END, "Esperado 'end' para fechar o if");
    }
  }
  else
  {
    // forma de linha única: corpo é UM statement, sem 'end'
    then_branch = statement(p);
  }

  Stmt *s = new_stmt(STMT_IF, line);
  s->as.if_stmt.condition = condition;
  s->as.if_stmt.then_branch = then_branch;
  s->as.if_stmt.else_branch = else_branch;
  return s;
}
static Stmt *while_statement(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'while'

  Expr *condition = expression(p);

  skip_newlines(p);

  Stmt *body = parse_block_body(p);
  Stmt *s = new_stmt(STMT_WHILE, line);
  s->as.while_stmt.condition = condition;
  s->as.while_stmt.body = body;
  return s;
}
// foreach <iterable> as <var> ... end

static Stmt *foreach_statement(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'foreach'

  Expr *iterable = expression(p);

  consume(p, TOKEN_AS, "Esperado 'as' no foreach");

  consume(p, TOKEN_IDENTIFIER, "Esperado nome da variavel apos 'as'");

  char *var_name = strdup(p->previous.lexeme);

  skip_newlines(p);

  Stmt *body = parse_block_body(p);
  Stmt *s = new_stmt(STMT_FOREACH, line);
  s->as.foreach_stmt.iterable = iterable;
  s->as.foreach_stmt.var_name = var_name;
  s->as.foreach_stmt.body = body;
  return s;
}
// for <var> in <iterable> ... end

static Stmt *for_statement(Parser *p)
{

  int line = p->current.line;

  advance_parser(p); // consome 'for'

  consume(p, TOKEN_IDENTIFIER, "Esperado nome da variavel no for");

  char *var_name = strdup(p->previous.lexeme);

  consume(p, TOKEN_IN, "Esperado 'in' no for");

  Expr *iterable = expression(p);

  skip_newlines(p);

  Stmt *body = parse_block_body(p);
  Stmt *s = new_stmt(STMT_FOR, line);
  s->as.for_stmt.var_name = var_name;
  s->as.for_stmt.iterable = iterable;
  s->as.for_stmt.body = body;
  return s;
}
static Stmt *expression_statement(Parser *p)
{

  int line = p->current.line;

  Expr *expr = expression(p);

  Stmt *s = new_stmt(STMT_EXPRESSION, line);

  s->as.expr_stmt.expression = expr;

  return s;
}
static Stmt *statement(Parser *p)
{

  if (check(p, TOKEN_RETURN))
    return return_statement(p);

  if (check(p, TOKEN_IF))
    return if_statement(p);

  if (check(p, TOKEN_WHILE))
    return while_statement(p);

  if (check(p, TOKEN_FOREACH))
    return foreach_statement(p);

  if (check(p, TOKEN_FOR))
    return for_statement(p);

  return expression_statement(p);
}
// Recuperação de erro: pula até um ponto seguro (newline ou início de bloco)

static void synchronize(Parser *p)
{

  p->panic_mode = 0;

  while (!check(p, TOKEN_EOF))
  {

    if (p->previous.type == TOKEN_NEWLINE)
      return;

    switch (p->current.type)
    {

    case TOKEN_FUNC:
    case TOKEN_CONST:
    case TOKEN_IF:

    case TOKEN_FOR:
    case TOKEN_FOREACH:
    case TOKEN_WHILE:

    case TOKEN_RETURN:
    case TOKEN_CLASS:

      return;

    default:;
    }

    advance_parser(p);
  }
}

// =================================================================
//  OOP: classes, interfaces, traits, enums
// =================================================================

// Lê uma sequência de modificadores e devolve o bitmask.
// public private protected readonly static (em qualquer ordem)
static int parse_modifiers(Parser *p)
{
  int mods = MOD_NONE;
  for (;;)
  {
    if (match_tok(p, TOKEN_PUBLIC))
      mods |= MOD_PUBLIC;
    else if (match_tok(p, TOKEN_PRIVATE))
      mods |= MOD_PRIVATE;
    else if (match_tok(p, TOKEN_PROTECTED))
      mods |= MOD_PROTECTED;
    else if (match_tok(p, TOKEN_READONLY))
      mods |= MOD_READONLY;
    else if (match_tok(p, TOKEN_STATIC))
      mods |= MOD_STATIC;
    else
      break;
  }
  return mods;
}

// Lê UM membro de classe/interface/trait.
// allow_body=1 -> métodos têm corpo (classe/trait)
// allow_body=0 -> métodos são só assinatura (interface)
static Member *parse_member(Parser *p, int allow_body)
{
  Member *m = (Member *)malloc(sizeof(Member));
  m->is_method = 0;
  m->modifiers = MOD_NONE;
  m->name = NULL;
  m->field_type = NULL;
  m->field_init = NULL;
  m->params = NULL;
  m->param_count = 0;
  m->return_type = NULL;
  m->body = NULL;

  // Modificadores são opcionais; default é public (resolvido depois na semântica)
  m->modifiers = parse_modifiers(p);

  // 'construct' é um método especial (o construtor)
  if (check(p, TOKEN_CONSTRUCT))
  {
    m->is_method = 1;
    m->name = strdup("construct");
    advance_parser(p);
  }
  else if (check(p, TOKEN_FUNC))
  {
    m->is_method = 1;
    advance_parser(p);
    consume(p, TOKEN_IDENTIFIER, "Esperado nome do metodo");
    m->name = strdup(p->previous.lexeme);
  }
  else
  {
    // É um campo: nome [: tipo] [= valor]
    consume(p, TOKEN_IDENTIFIER, "Esperado nome do membro");
    m->name = strdup(p->previous.lexeme);
    // tipo é opcional (em strict, o checker exige; no dinamico, e livre)
    if (match_tok(p, TOKEN_COLON))
    {
      m->field_type = parse_type(p);
    }
    if (match_tok(p, TOKEN_EQUALS))
      m->field_init = expression(p);
    return m;
  }

  // Aqui é método: parâmetros (parênteses opcionais)
  int has_paren = match_tok(p, TOKEN_LPAREN);
  int has_params_no_paren =
      !has_paren && check(p, TOKEN_IDENTIFIER) && check_next(p, TOKEN_COLON);

  if (has_paren || has_params_no_paren)
  {
    if (!(has_paren && check(p, TOKEN_RPAREN)))
    {
      int capacity = 0;
      do
      {
        Param param = parse_param(p);
        if (m->param_count + 1 > capacity)
        {
          capacity = capacity < 4 ? 4 : capacity * 2;
          m->params = realloc(m->params, sizeof(Param) * capacity);
        }
        m->params[m->param_count++] = param;
      } while (match_tok(p, TOKEN_COMMA));
    }
    if (has_paren)
      consume(p, TOKEN_RPAREN, "Esperado ')' apos os parametros");
  }

  if (match_tok(p, TOKEN_ARROW))
    m->return_type = parse_type(p);

  if (allow_body)
  {
    skip_newlines(p);
    m->body = parse_block_body(p); // consome o 'end'
  }
  // interface: sem corpo, nada a consumir

  return m;
}

// Helper genérico para acumular membros até 'end'
static Member **parse_members_until_end(Parser *p, int *out_count, int allow_body)
{
  Member **members = NULL;
  int count = 0, capacity = 0;

  skip_newlines(p);
  while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF))
  {
    Member *m = parse_member(p, allow_body);
    if (count + 1 > capacity)
    {
      capacity = capacity < 4 ? 4 : capacity * 2;
      members = realloc(members, sizeof(Member *) * capacity);
    }
    members[count++] = m;
    skip_newlines(p);
  }
  consume(p, TOKEN_END, "Esperado 'end' para fechar o bloco");
  *out_count = count;
  return members;
}

static Stmt *class_declaration(Parser *p)
{
  int line = p->current.line;
  advance_parser(p); // consome 'class'
  consume(p, TOKEN_IDENTIFIER, "Esperado nome da classe");
  char *name = strdup(p->previous.lexeme);

  char *superclass = NULL;
  char **interfaces = NULL;
  int interface_count = 0;

  // extends vem ANTES de implements
  if (match_tok(p, TOKEN_EXTENDS))
  {
    consume(p, TOKEN_IDENTIFIER, "Esperado nome da superclasse apos 'extends'");
    superclass = strdup(p->previous.lexeme);
  }
  if (match_tok(p, TOKEN_IMPLEMENTS))
  {
    int capacity = 0;
    do
    {
      consume(p, TOKEN_IDENTIFIER, "Esperado nome da interface");
      if (interface_count + 1 > capacity)
      {
        capacity = capacity < 4 ? 4 : capacity * 2;
        interfaces = realloc(interfaces, sizeof(char *) * capacity);
      }
      interfaces[interface_count++] = strdup(p->previous.lexeme);
    } while (match_tok(p, TOKEN_COMMA));
  }

  skip_newlines(p);

  // Corpo: 'use Trait' aparece no topo, depois campos e métodos
  char **traits = NULL;
  int trait_count = 0, trait_cap = 0;
  Member **members = NULL;
  int member_count = 0, member_cap = 0;

  while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF))
  {
    if (check(p, TOKEN_USE))
    {
      advance_parser(p);
      consume(p, TOKEN_IDENTIFIER, "Esperado nome do trait apos 'use'");
      if (trait_count + 1 > trait_cap)
      {
        trait_cap = trait_cap < 4 ? 4 : trait_cap * 2;
        traits = realloc(traits, sizeof(char *) * trait_cap);
      }
      traits[trait_count++] = strdup(p->previous.lexeme);
      skip_newlines(p);
      continue;
    }
    Member *m = parse_member(p, 1);
    if (member_count + 1 > member_cap)
    {
      member_cap = member_cap < 4 ? 4 : member_cap * 2;
      members = realloc(members, sizeof(Member *) * member_cap);
    }
    members[member_count++] = m;
    skip_newlines(p);
  }
  consume(p, TOKEN_END, "Esperado 'end' para fechar a classe");

  Stmt *s = new_stmt(STMT_CLASS, line);
  s->as.class_decl.name = name;
  s->as.class_decl.superclass = superclass;
  s->as.class_decl.interfaces = interfaces;
  s->as.class_decl.interface_count = interface_count;
  s->as.class_decl.traits = traits;
  s->as.class_decl.trait_count = trait_count;
  s->as.class_decl.members = members;
  s->as.class_decl.member_count = member_count;
  return s;
}

static Stmt *interface_declaration(Parser *p)
{
  int line = p->current.line;
  advance_parser(p); // consome 'interface'
  consume(p, TOKEN_IDENTIFIER, "Esperado nome da interface");
  char *name = strdup(p->previous.lexeme);
  skip_newlines(p);

  int count = 0;
  Member **members = parse_members_until_end(p, &count, 0); // sem corpo

  Stmt *s = new_stmt(STMT_INTERFACE, line);
  s->as.interface_decl.name = name;
  s->as.interface_decl.members = members;
  s->as.interface_decl.member_count = count;
  return s;
}

static Stmt *trait_declaration(Parser *p)
{
  int line = p->current.line;
  advance_parser(p); // consome 'trait'
  consume(p, TOKEN_IDENTIFIER, "Esperado nome do trait");
  char *name = strdup(p->previous.lexeme);
  skip_newlines(p);

  int count = 0;
  Member **members = parse_members_until_end(p, &count, 1); // com corpo

  Stmt *s = new_stmt(STMT_TRAIT, line);
  s->as.trait_decl.name = name;
  s->as.trait_decl.members = members;
  s->as.trait_decl.member_count = count;
  return s;
}

static Stmt *enum_declaration(Parser *p)
{
  int line = p->current.line;
  advance_parser(p); // consome 'enum'
  consume(p, TOKEN_IDENTIFIER, "Esperado nome do enum");
  char *name = strdup(p->previous.lexeme);
  skip_newlines(p);

  char **values = NULL;
  int count = 0, capacity = 0;

  while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF))
  {
    consume(p, TOKEN_IDENTIFIER, "Esperado nome do valor do enum");
    if (count + 1 > capacity)
    {
      capacity = capacity < 4 ? 4 : capacity * 2;
      values = realloc(values, sizeof(char *) * capacity);
    }
    values[count++] = strdup(p->previous.lexeme);
    match_tok(p, TOKEN_COMMA); // vírgula opcional entre valores
    skip_newlines(p);
  }
  consume(p, TOKEN_END, "Esperado 'end' para fechar o enum");

  Stmt *s = new_stmt(STMT_ENUM, line);
  s->as.enum_decl.name = name;
  s->as.enum_decl.values = values;
  s->as.enum_decl.value_count = count;
  return s;
}

static Stmt *namespace_declaration(Parser *p)
{
  int line = p->current.line;
  advance_parser(p); // consome 'namespace'

  // App.Service.Sub -> junta tudo numa string
  consume(p, TOKEN_IDENTIFIER, "Esperado nome do namespace");
  char buffer[256];
  strcpy(buffer, p->previous.lexeme);
  while (match_tok(p, TOKEN_DOT))
  {
    consume(p, TOKEN_IDENTIFIER, "Esperado nome apos '.'");
    strcat(buffer, ".");
    strcat(buffer, p->previous.lexeme);
  }

  Stmt *s = new_stmt(STMT_NAMESPACE, line);
  s->as.namespace_decl.name = strdup(buffer);
  return s;
}

static Stmt *import_declaration(Parser *p)
{
  int line = p->current.line;
  advance_parser(p); // consome 'import'
  consume(p, TOKEN_IDENTIFIER, "Esperado nome a importar");
  char *what = strdup(p->previous.lexeme);

  consume(p, TOKEN_FROM, "Esperado 'from' no import");

  consume(p, TOKEN_IDENTIFIER, "Esperado origem do import");
  char buffer[256];
  strcpy(buffer, p->previous.lexeme);
  while (match_tok(p, TOKEN_DOT))
  {
    consume(p, TOKEN_IDENTIFIER, "Esperado nome apos '.'");
    strcat(buffer, ".");
    strcat(buffer, p->previous.lexeme);
  }

  Stmt *s = new_stmt(STMT_IMPORT, line);
  s->as.import_decl.name = what;
  s->as.import_decl.from = strdup(buffer);
  return s;
}

// =================================================================

//  ENTRADA

// =================================================================

Stmt *parse(const char *source, int *had_error)
{

  Lexer lexer;

  lexer_init(&lexer, source);
  Parser p;
  p.lexer = &lexer;
  p.had_error = 0;
  p.panic_mode = 0;
  // Preenche current e next
  p.current = lexer_next_token(&lexer);
  p.next = lexer_next_token(&lexer);

  Stmt *program = new_stmt(STMT_BLOCK, 1);
  program->as.block.statements = NULL;
  program->as.block.count = 0;
  int capacity = 0;

  skip_newlines(&p);
  while (!check(&p, TOKEN_EOF))
  {
    Stmt *s = declaration(&p);
    if (s)
    {
      if (program->as.block.count + 1 > capacity)
      {
        capacity = capacity < 8 ? 8 : capacity * 2;
        program->as.block.statements =
            realloc(program->as.block.statements, sizeof(Stmt *) * capacity);
      }
      program->as.block.statements[program->as.block.count++] = s;
    }
    skip_newlines(&p);
  }

  *had_error = p.had_error;
  return program;
}