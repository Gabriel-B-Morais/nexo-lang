#ifndef NEXO_CHECKER_H
#define NEXO_CHECKER_H

#include "ast.h"

// Verifica os tipos do programa.
// strict_mode = 1 se o arquivo começa com #!strict.
// Retorna o número de erros encontrados (0 = tudo certo).
int check_types(Stmt *program, int strict_mode);

// Detecta se o código-fonte ativa o modo strict.
int detect_strict_mode(const char *source);

#endif