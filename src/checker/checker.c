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
  char names[256][64];
  CType types[256];
  char class_names[256][64];
  int count;
  struct Scope *parent;
} Scope;

// Estado do checker
typedef struct
{
  int strict;
  int errors;
  Scope *scope;
  const char *current_class;
  Stmt **classes;
  int class_count;
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

static void scope_define(Scope *s, const char *name, CType type, const char *class_name)
{
  for (int i = 0; i < s->count; i++)
  {
    if (strcmp(s->names[i], name) == 0)
    {
      s->types[i] = type;
      if (class_name)
        strncpy(s->class_names[i], class_name, 63);
      else
        s->class_names[i][0] = '\0';
      s->class_names[i][63] = '\0';
      return;
    }
  }
  if (s->count < 256)
  {
    strncpy(s->names[s->count], name, 63);
    s->names[s->count][63] = '\0';
    s->types[s->count] = type;
    if (class_name)
      strncpy(s->class_names[s->count], class_name, 63);
    else
      s->class_names[s->count][0] = '\0';
    s->class_names[s->count][63] = '\0';
    s->count++;
  }
}

// Busca o tipo de uma variável (sobe pelos escopos). Retorna 1 se achou.
// Se out_class != NULL, preenche com o nome da classe (string vazia se não for instância).
static int scope_lookup(Scope *s, const char *name, CType *out, const char **out_class)
{
  for (Scope *cur = s; cur != NULL; cur = cur->parent)
  {
    for (int i = 0; i < cur->count; i++)
    {
      if (strcmp(cur->names[i], name) == 0)
      {
        if (out)
          *out = cur->types[i];
        if (out_class)
          *out_class = cur->class_names[i];
        return 1;
      }
    }
  }
  return 0;
}

// Acha a declaração de uma classe pelo nome
static Stmt *find_class_decl(Checker *c, const char *name)
{
  for (int i = 0; i < c->class_count; i++)
  {
    if (strcmp(c->classes[i]->as.class_decl.name, name) == 0)
      return c->classes[i];
  }
  return NULL;
}

// Acha um membro (campo ou método) numa classe, subindo a herança.
// Preenche *owner com o nome da classe onde o membro foi achado.
static Member *find_member_decl(Checker *c, const char *class_name,
                                const char *member_name, const char **owner)
{
  const char *cur = class_name;
  while (cur != NULL)
  {
    Stmt *decl = find_class_decl(c, cur);
    if (!decl)
      break;
    for (int i = 0; i < decl->as.class_decl.member_count; i++)
    {
      Member *m = decl->as.class_decl.members[i];
      if (strcmp(m->name, member_name) == 0)
      {
        if (owner)
          *owner = cur;
        return m;
      }
    }
    cur = decl->as.class_decl.superclass; // sobe na herança
  }
  return NULL;
}

// 'sub' é subclasse de 'base' (ou a própria)?
static int is_subclass_of(Checker *c, const char *sub, const char *base)
{
  const char *cur = sub;
  while (cur != NULL)
  {
    if (strcmp(cur, base) == 0)
      return 1;
    Stmt *decl = find_class_decl(c, cur);
    if (!decl)
      break;
    cur = decl->as.class_decl.superclass;
  }
  return 0;
}

// Decide se o acesso a um membro é permitido a partir do contexto atual.
// owner = classe que declara o membro. Retorna 1 se permitido.
static int can_access(Checker *c, int modifiers, const char *owner)
{
  // public (ou sem modificador, que é public por padrão) sempre pode
  int is_private = (modifiers & MOD_PRIVATE) != 0;
  int is_protected = (modifiers & MOD_PROTECTED) != 0;

  if (!is_private && !is_protected)
    return 1; // public

  // private: só dentro da própria classe dona
  if (is_private)
  {
    return c->current_class != NULL &&
           strcmp(c->current_class, owner) == 0;
  }

  // protected: na classe dona ou em subclasses dela
  if (is_protected)
  {
    return c->current_class != NULL &&
           is_subclass_of(c, c->current_class, owner);
  }

  return 1;
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
    if (scope_lookup(c->scope, expr->as.string_value, &t, NULL))
      return t;
    return T_ANY;
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

    // Descobre se o valor é uma instância de classe (new Classe())
    const char *inst_class = NULL;
    if (stmt->as.var_decl.initializer &&
        stmt->as.var_decl.initializer->type == EXPR_CALL)
    {
      Expr *callee = stmt->as.var_decl.initializer->as.call.callee;
      if (callee->type == EXPR_IDENTIFIER &&
          find_class_decl(c, callee->as.string_value))
      {
        inst_class = callee->as.string_value;
      }
    }
    scope_define(c->scope, stmt->as.var_decl.name, var_type, inst_class);

    break;
  }

  case STMT_BLOCK:
    for (int i = 0; i < stmt->as.block.count; i++)
      check_stmt(c, stmt->as.block.statements[i]);
    break;

  case STMT_IF:
    check_expr(c, stmt->as.if_stmt.condition);
    check_stmt(c, stmt->as.if_stmt.then_branch);
    if (stmt->as.if_stmt.else_branch)
      check_stmt(c, stmt->as.if_stmt.else_branch);
    break;

  case STMT_WHILE:
    check_expr(c, stmt->as.while_stmt.condition);
    check_stmt(c, stmt->as.while_stmt.body);
    break;

  case STMT_FOREACH:
    check_expr(c, stmt->as.foreach_stmt.iterable);
    check_stmt(c, stmt->as.foreach_stmt.body);
    break;

  case STMT_FOR:
    check_expr(c, stmt->as.for_stmt.iterable);
    check_stmt(c, stmt->as.for_stmt.body);
    break;

  case STMT_FUNC_DECL:
  {
    if (c->strict)
    {
      if (!stmt->as.func_decl.return_type)
      {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "func '%s': tipo de retorno obrigatorio em strict",
                 stmt->as.func_decl.name);
        type_error(c, stmt->line, msg);
      }
      for (int i = 0; i < stmt->as.func_decl.param_count; i++)
      {
        if (!stmt->as.func_decl.params[i].type)
        {
          char msg[160];
          snprintf(msg, sizeof(msg),
                   "func '%s': parametro '%s' sem tipo em strict",
                   stmt->as.func_decl.name,
                   stmt->as.func_decl.params[i].name);
          type_error(c, stmt->line, msg);
        }
      }
    }
    check_stmt(c, stmt->as.func_decl.body);
    break;
  }
  case STMT_EXPRESSION:
    check_expr(c, stmt->as.expr_stmt.expression);
    break;
  case STMT_RETURN:
    if (stmt->as.ret.value)
      check_expr(c, stmt->as.ret.value);
    break;
  case STMT_CLASS:
  {
    if (c->strict)
    {
      for (int i = 0; i < stmt->as.class_decl.member_count; i++)
      {
        Member *m = stmt->as.class_decl.members[i];
        if (!m->is_method)
        {
          if (!m->field_type)
          {
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "classe '%s': campo '%s' sem tipo em strict",
                     stmt->as.class_decl.name, m->name);
            type_error(c, stmt->line, msg);
          }
        }
        else
        {
          // método: parâmetros e retorno obrigatórios
          // (exceto construct, que não tem retorno)
          int is_ctor = strcmp(m->name, "construct") == 0;
          if (!is_ctor && !m->return_type)
          {
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "classe '%s': metodo '%s' sem tipo de retorno em strict",
                     stmt->as.class_decl.name, m->name);
            type_error(c, stmt->line, msg);
          }
          for (int j = 0; j < m->param_count; j++)
          {
            if (!m->params[j].type)
            {
              char msg[160];
              snprintf(msg, sizeof(msg),
                       "classe '%s': metodo '%s', parametro '%s' sem tipo em strict",
                       stmt->as.class_decl.name, m->name, m->params[j].name);
              type_error(c, stmt->line, msg);
            }
          }
        }
      }
    }
    // Percorre os corpos dos métodos com o contexto de classe ativo
    const char *prev_class = c->current_class;
    c->current_class = stmt->as.class_decl.name;
    for (int i = 0; i < stmt->as.class_decl.member_count; i++)
    {
      Member *m = stmt->as.class_decl.members[i];
      if (m->is_method && m->body)
      {
        // novo escopo para o método, com 'this' disponível
        Scope *method_scope = scope_new(c->scope);
        Scope *saved = c->scope;
        c->scope = method_scope;
        // registra 'this' como a própria classe (tipo any por ora)
        scope_define(c->scope, "this", T_ANY, stmt->as.class_decl.name);
        // registra os parâmetros
        for (int j = 0; j < m->param_count; j++)
        {
          CType pt = m->params[j].type ? type_to_ctype(m->params[j].type) : T_ANY;
          scope_define(c->scope, m->params[j].name, pt, NULL);
        }
        check_stmt(c, m->body);
        c->scope = saved;
      }
    }
    c->current_class = prev_class;
    break;
  }
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
      if (scope_lookup(c->scope, target->as.string_value, &existing, NULL))
      {
        CType new_type = infer_type(c, expr->as.assign.value);
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

      // Se o valor é new Classe(), registra a variável como instância
      const char *inst_class = NULL;
      if (expr->as.assign.value->type == EXPR_CALL)
      {
        Expr *callee = expr->as.assign.value->as.call.callee;
        if (callee->type == EXPR_IDENTIFIER &&
            find_class_decl(c, callee->as.string_value))
        {
          inst_class = callee->as.string_value;
        }
      }
      if (inst_class)
      {
        scope_define(c->scope, target->as.string_value, T_ANY, inst_class);
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

  case EXPR_GET:
  {
    // valida o objeto à esquerda primeiro
    check_expr(c, expr->as.get.object);

    Expr *obj = expr->as.get.object;
    const char *target_class = NULL;

    // Caso 1: this.membro  -> classe é a current_class
    if (obj->type == EXPR_IDENTIFIER &&
        strcmp(obj->as.string_value, "this") == 0)
    {
      target_class = c->current_class;
    }
    // Caso 2: Classe.membro  -> o identificador é o nome de uma classe
    // Caso 3: variavel.membro -> a variavel guarda uma instancia
    else if (obj->type == EXPR_IDENTIFIER)
    {
      if (find_class_decl(c, obj->as.string_value))
      {
        target_class = obj->as.string_value; // acesso estatico
      }
      else
      {
        // procura a variavel na tabela e ve se ela e instancia de alguma classe
        const char *var_class = NULL;
        if (scope_lookup(c->scope, obj->as.string_value, NULL, &var_class))
        {
          if (var_class && var_class[0] != '\0')
            target_class = var_class;
        }
      }
    }

    // Se conseguimos determinar a classe, checa visibilidade
    if (target_class != NULL)
    {
      const char *owner = NULL;
      Member *m = find_member_decl(c, target_class, expr->as.get.name, &owner);
      if (m && !can_access(c, m->modifiers, owner))
      {
        const char *vis = (m->modifiers & MOD_PRIVATE) ? "private" : "protected";
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "membro '%s' e %s da classe '%s' e nao pode ser acessado aqui",
                 expr->as.get.name, vis, owner);
        type_error(c, expr->line, msg);
      }
    }
    break;
  }

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
  c.current_class = NULL;
  c.classes = NULL;
  c.class_count = 0;

  // coleta todas as classes do programa (para resolver visibilidade)
  int cap = 0;
  for (int i = 0; i < program->as.block.count; i++)
  {
    Stmt *s = program->as.block.statements[i];
    if (s->type == STMT_CLASS)
    {
      if (c.class_count + 1 > cap)
      {
        cap = cap < 8 ? 8 : cap * 2;
        c.classes = realloc(c.classes, sizeof(Stmt *) * cap);
      }
      c.classes[c.class_count++] = s;
    }
  }

  for (int i = 0; i < program->as.block.count; i++)
  {
    check_stmt(&c, program->as.block.statements[i]);
  }

  return c.errors;
}