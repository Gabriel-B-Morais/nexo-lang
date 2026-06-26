#ifndef NEXO_INTERPRETER_H
#define NEXO_INTERPRETER_H

#include "ast.h"

// Executa um programa (STMT_BLOCK vindo do parser).
// Retorna 0 em sucesso, 1 se houve erro de runtime.
int interpret(Stmt* program);

#endif