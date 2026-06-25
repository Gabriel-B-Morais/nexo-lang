#ifndef NEXO_AST_H
#define NEXO_AST_H

#include "token.h"

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Type Type;

// ---------- Tipos ----------
typedef enum
{
  TYPE_SIMPLE,
  TYPE_UNION,
  TYPE_GENERIC
} TypeKind;

struct Type
{
  TypeKind kind;
  union
  {
    char *name;
    struct
    {
      Type **members;
      int count;
    } union_type;
    struct
    {
      char *name;
      Type **args;
      int arg_count;
    } generic;
  } as;
};

typedef struct
{
  char *name;
  Type *type;
} Param;

// ---------- Modificadores (bitmask) ----------
// Combináveis: PRIVATE | STATIC, PUBLIC | READONLY, etc.
typedef enum
{
  MOD_NONE = 0,
  MOD_PUBLIC = 1 << 0,
  MOD_PRIVATE = 1 << 1,
  MOD_PROTECTED = 1 << 2,
  MOD_READONLY = 1 << 3,
  MOD_STATIC = 1 << 4
} Modifier;

// ---------- Expressões ----------
typedef enum
{
  EXPR_INT_LITERAL,
  EXPR_FLOAT_LITERAL,
  EXPR_STRING_LITERAL,
  EXPR_BOOL_LITERAL,
  EXPR_IDENTIFIER,
  EXPR_BINARY,
  EXPR_UNARY,
  EXPR_POSTFIX,
  EXPR_ASSIGN,
  EXPR_CALL,
  EXPR_GET,
  EXPR_GROUPING,
  EXPR_ARRAY,
  EXPR_OBJECT
} ExprType;

struct Expr
{
  ExprType type;
  int line;
  union
  {
    long long int_value;
    double float_value;
    char *string_value;
    int bool_value;
    struct
    {
      Expr *left;
      TokenType op;
      Expr *right;
    } binary;
    struct
    {
      TokenType op;
      Expr *operand;
    } unary;
    struct
    {
      Expr *operand;
      TokenType op;
    } postfix;
    struct
    {
      Expr *target;
      Expr *value;
    } assign;
    struct
    {
      Expr *callee;
      Expr **args;
      int arg_count;
    } call;
    struct
    {
      Expr *object;
      char *name;
    } get;
    struct
    {
      Expr *inner;
    } grouping;
    struct
    {
      Expr **elements;
      int count;
    } array;
    struct
    {
      char **keys;
      Expr **values;
      int count;
    } object;
  } as;
};

// ---------- Membros de classe ----------
// Um campo (propriedade) ou um método dentro de uma classe.
typedef struct
{
  int is_method; // 1 = método, 0 = campo
  int modifiers; // bitmask de Modifier
  char *name;

  // se for campo:
  Type *field_type;
  Expr *field_init; // pode ser NULL

  // se for método:
  Param *params;
  int param_count;
  Type *return_type;
  Stmt *body; // NULL se for assinatura de interface
} Member;

// ---------- Statements ----------
typedef enum
{
  STMT_VAR_DECL,
  STMT_CONST_DECL,
  STMT_FUNC_DECL,
  STMT_RETURN,
  STMT_IF,
  STMT_WHILE,
  STMT_FOREACH,
  STMT_FOR,
  STMT_EXPRESSION,
  STMT_BLOCK,
  STMT_CLASS,
  STMT_INTERFACE,
  STMT_TRAIT,
  STMT_ENUM,
  STMT_NAMESPACE,
  STMT_IMPORT
} StmtType;

struct Stmt
{
  StmtType type;
  int line;
  union
  {
    struct
    {
      char *name;
      Type *var_type;
      Expr *initializer;
    } var_decl;
    struct
    {
      char *name;
      Param *params;
      int param_count;
      Type *return_type;
      Stmt *body;
    } func_decl;
    struct
    {
      Expr *value;
    } ret;
    struct
    {
      Expr *condition;
      Stmt *then_branch;
      Stmt *else_branch;
    } if_stmt;
    struct
    {
      Expr *condition;
      Stmt *body;
    } while_stmt;
    struct
    {
      Expr *iterable;
      char *var_name;
      Stmt *body;
    } foreach_stmt;
    struct
    {
      char *var_name;
      Expr *iterable;
      Stmt *body;
    } for_stmt;
    struct
    {
      Expr *expression;
    } expr_stmt;
    struct
    {
      Stmt **statements;
      int count;
    } block;

    // class Nome extends Base implements I1, I2 ... end
    struct
    {
      char *name;
      char *superclass;  // NULL se não houver extends
      char **interfaces; // implements I1, I2...
      int interface_count;
      char **traits; // use T1; use T2
      int trait_count;
      Member **members;
      int member_count;
    } class_decl;

    // interface Nome ... end  (só assinaturas de método)
    struct
    {
      char *name;
      Member **members;
      int member_count;
    } interface_decl;

    // trait Nome ... end (campos + métodos com corpo)
    struct
    {
      char *name;
      Member **members;
      int member_count;
    } trait_decl;

    // enum Nome { A, B, C } ... end
    struct
    {
      char *name;
      char **values;
      int value_count;
    } enum_decl;

    // namespace App.Service
    struct
    {
      char *name;
    } namespace_decl;

    // import Pessoa from App.Model
    struct
    {
      char *name;
      char *from;
    } import_decl;
  } as;
};

void ast_print(Stmt *program);

#endif