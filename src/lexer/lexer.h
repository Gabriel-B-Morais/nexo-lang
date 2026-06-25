#ifndef NEXO_LEXER_H
#define NEXO_LEXER_H

#include "token.h"

typedef struct {
  const char* source;  // código fonte inteiro
  int start;           // inicio do token atual
  int current;         // posição atual da leitura
  int line;            // linha atual
  int column;          // coluna atual
} Lexer;

// Inicializa o lexer com o código fonte
void lexer_init(Lexer* lexer, const char* source);

// Produz o próximo token
Token lexer_next_token(Lexer* lexer);

#endif