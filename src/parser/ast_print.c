#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

static void print_expr(Expr *e);
static void print_stmt(Stmt *s, int indent);
static void print_member(Member *m, int indent);
static void print_type(Type* t);

static void pad(int n)
{
  for (int i = 0; i < n; i++)
    printf("  ");
}

static void print_modifiers(int mods)
{
  if (mods & MOD_PUBLIC)
    printf("public ");
  if (mods & MOD_PRIVATE)
    printf("private ");
  if (mods & MOD_PROTECTED)
    printf("protected ");
  if (mods & MOD_READONLY)
    printf("readonly ");
  if (mods & MOD_STATIC)
    printf("static ");
  if (mods == MOD_NONE)
    printf("(public) "); // default implícito
}

static void print_member(Member *m, int indent)
{
  pad(indent);
  print_modifiers(m->modifiers);
  if (m->is_method)
  {
    printf("METHOD %s(", m->name);
    for (int i = 0; i < m->param_count; i++)
    {
      if (i > 0)
        printf(", ");
      printf("%s: ", m->params[i].name);
      print_type(m->params[i].type);
    }
    printf(") -> ");
    print_type(m->return_type);
    printf("\n");
    if (m->body)
      print_stmt(m->body, indent + 1);
  }
  else
  {
    printf("FIELD %s: ", m->name);
    print_type(m->field_type);
    if (m->field_init)
    {
      printf(" = ");
      print_expr(m->field_init);
    }
    printf("\n");
  }
}

static const char *op_str(TokenType op)
{
  switch (op)
  {
  case TOKEN_PLUS:
    return "+";
  case TOKEN_MINUS:
    return "-";
  case TOKEN_STAR:
    return "*";
  case TOKEN_SLASH:
    return "/";
  case TOKEN_EQ_EQ:
    return "==";
  case TOKEN_BANG_EQ:
    return "!=";
  case TOKEN_LESS:
    return "<";
  case TOKEN_LESS_EQ:
    return "<=";
  case TOKEN_GREATER:
    return ">";
  case TOKEN_GREATER_EQ:
    return ">=";
  case TOKEN_AND:
    return "&&";
  case TOKEN_OR:
    return "||";
  case TOKEN_BANG:
    return "!";
  case TOKEN_PLUS_PLUS:
    return "++";
  case TOKEN_MINUS_MINUS:
    return "--";
  default:
    return "?";
  }
}

static void print_type(Type *t)
{
  if (!t)
  {
    printf("<sem tipo>");
    return;
  }
  switch (t->kind)
  {
  case TYPE_SIMPLE:
    printf("%s", t->as.name);
    break;
  case TYPE_UNION:
    for (int i = 0; i < t->as.union_type.count; i++)
    {
      if (i > 0)
        printf("|");
      print_type(t->as.union_type.members[i]);
    }
    break;
  case TYPE_GENERIC:
    printf("%s<", t->as.generic.name);
    for (int i = 0; i < t->as.generic.arg_count; i++)
    {
      if (i > 0)
        printf(",");
      print_type(t->as.generic.args[i]);
    }
    printf(">");
    break;
  }
}

static void print_expr(Expr *e)
{
  if (!e)
  {
    printf("null");
    return;
  }
  switch (e->type)
  {
  case EXPR_INT_LITERAL:
    printf("%lld", e->as.int_value);
    break;
  case EXPR_FLOAT_LITERAL:
    printf("%g", e->as.float_value);
    break;
  case EXPR_STRING_LITERAL:
    printf("\"%s\"", e->as.string_value);
    break;
  case EXPR_BOOL_LITERAL:
    printf("%s", e->as.bool_value ? "true" : "false");
    break;
  case EXPR_IDENTIFIER:
    printf("%s", e->as.string_value);
    break;
  case EXPR_BINARY:
    printf("(");
    print_expr(e->as.binary.left);
    printf(" %s ", op_str(e->as.binary.op));
    print_expr(e->as.binary.right);
    printf(")");
    break;
  case EXPR_UNARY:
    printf("(%s", op_str(e->as.unary.op));
    print_expr(e->as.unary.operand);
    printf(")");
    break;
  case EXPR_POSTFIX:
    print_expr(e->as.postfix.operand);
    printf("%s", op_str(e->as.postfix.op));
    break;
  case EXPR_ASSIGN:
    print_expr(e->as.assign.target);
    printf(" = ");
    print_expr(e->as.assign.value);
    break;
  case EXPR_CALL:
    print_expr(e->as.call.callee);
    printf("(");
    for (int i = 0; i < e->as.call.arg_count; i++)
    {
      if (i > 0)
        printf(", ");
      print_expr(e->as.call.args[i]);
    }
    printf(")");
    break;
  case EXPR_GET:
    print_expr(e->as.get.object);
    printf(".%s", e->as.get.name);
    break;
  case EXPR_GROUPING:
    printf("(");
    print_expr(e->as.grouping.inner);
    printf(")");
    break;
  case EXPR_ARRAY:
    printf("[");
    for (int i = 0; i < e->as.array.count; i++)
    {
      if (i > 0)
        printf(", ");
      print_expr(e->as.array.elements[i]);
    }
    printf("]");
    break;
  case EXPR_OBJECT:
    printf("{");
    for (int i = 0; i < e->as.object.count; i++)
    {
      if (i > 0)
        printf(", ");
      printf("%s: ", e->as.object.keys[i]);
      print_expr(e->as.object.values[i]);
    }
    printf("}");
    break;
  }
}
static void print_stmt(Stmt *s, int indent)
{
  if (!s)
    return;

  // BLOCK não imprime ele mesmo, só itera os filhos
  if (s->type == STMT_BLOCK)
  {
    for (int i = 0; i < s->as.block.count; i++)
    {
      print_stmt(s->as.block.statements[i], indent);
    }
    return;
  }

  pad(indent);
  switch (s->type)
  {
  case STMT_VAR_DECL:
    printf("VAR %s: ", s->as.var_decl.name);
    print_type(s->as.var_decl.var_type);
    if (s->as.var_decl.initializer)
    {
      printf(" = ");
      print_expr(s->as.var_decl.initializer);
    }
    printf("\n");
    break;
  case STMT_CONST_DECL:
    printf("CONST %s: ", s->as.var_decl.name);
    print_type(s->as.var_decl.var_type);
    printf(" = ");
    print_expr(s->as.var_decl.initializer);
    printf("\n");
    break;
  case STMT_FUNC_DECL:
    printf("FUNC %s(", s->as.func_decl.name);
    for (int i = 0; i < s->as.func_decl.param_count; i++)
    {
      if (i > 0)
        printf(", ");
      printf("%s: ", s->as.func_decl.params[i].name);
      print_type(s->as.func_decl.params[i].type);
    }
    printf(") -> ");
    print_type(s->as.func_decl.return_type);
    printf("\n");
    print_stmt(s->as.func_decl.body, indent + 1);
    break;
  case STMT_RETURN:
    printf("RETURN ");
    print_expr(s->as.ret.value);
    printf("\n");
    break;
  case STMT_IF:
    printf("IF ");
    print_expr(s->as.if_stmt.condition);
    printf("\n");
    print_stmt(s->as.if_stmt.then_branch, indent + 1);
    if (s->as.if_stmt.else_branch)
    {
      pad(indent);
      printf("ELSE\n");
      print_stmt(s->as.if_stmt.else_branch, indent + 1);
    }
    break;
  case STMT_WHILE:
    printf("WHILE ");
    print_expr(s->as.while_stmt.condition);
    printf("\n");
    print_stmt(s->as.while_stmt.body, indent + 1);
    break;
  case STMT_FOREACH:
    printf("FOREACH ");
    print_expr(s->as.foreach_stmt.iterable);
    printf(" AS %s\n", s->as.foreach_stmt.var_name);
    print_stmt(s->as.foreach_stmt.body, indent + 1);
    break;
  case STMT_FOR:
    printf("FOR %s IN ", s->as.for_stmt.var_name);
    print_expr(s->as.for_stmt.iterable);
    printf("\n");
    print_stmt(s->as.for_stmt.body, indent + 1);
    break;
  case STMT_EXPRESSION:
    printf("EXPR ");
    print_expr(s->as.expr_stmt.expression);
    printf("\n");
    break;
  case STMT_BLOCK:
    break; // já tratado no topo
  case STMT_CLASS:
    printf("CLASS %s", s->as.class_decl.name);
    if (s->as.class_decl.superclass)
      printf(" extends %s", s->as.class_decl.superclass);
    for (int i = 0; i < s->as.class_decl.interface_count; i++)
    {
      printf("%s %s", i == 0 ? " implements" : ",",
             s->as.class_decl.interfaces[i]);
    }
    printf("\n");
    for (int i = 0; i < s->as.class_decl.trait_count; i++)
    {
      pad(indent + 1);
      printf("USE %s\n", s->as.class_decl.traits[i]);
    }
    for (int i = 0; i < s->as.class_decl.member_count; i++)
    {
      print_member(s->as.class_decl.members[i], indent + 1);
    }
    break;
  case STMT_INTERFACE:
    printf("INTERFACE %s\n", s->as.interface_decl.name);
    for (int i = 0; i < s->as.interface_decl.member_count; i++)
    {
      print_member(s->as.interface_decl.members[i], indent + 1);
    }
    break;
  case STMT_TRAIT:
    printf("TRAIT %s\n", s->as.trait_decl.name);
    for (int i = 0; i < s->as.trait_decl.member_count; i++)
    {
      print_member(s->as.trait_decl.members[i], indent + 1);
    }
    break;
  case STMT_ENUM:
    printf("ENUM %s\n", s->as.enum_decl.name);
    for (int i = 0; i < s->as.enum_decl.value_count; i++)
    {
      pad(indent + 1);
      printf("- %s\n", s->as.enum_decl.values[i]);
    }
    break;
  case STMT_NAMESPACE:
    printf("NAMESPACE %s\n", s->as.namespace_decl.name);
    break;
  case STMT_IMPORT:
    printf("IMPORT %s FROM %s\n",
           s->as.import_decl.name, s->as.import_decl.from);
    break;
  }
}

void ast_print(Stmt *program)
{
  printf("=== AST ===\n\n");
  print_stmt(program, 0);
}