#ifndef NEXO_PARSER_H
#define NEXO_PARSER_H

#include "ast.h"

// Faz o parse do código-fonte e retorna a AST (um STMT_BLOCK).
// Define *had_error como 1 se houve erro de sintaxe.
Stmt *parse(const char *source, int *had_error);

#endif