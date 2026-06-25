#ifndef NEXO_TOKEN_H
#define NEXO_TOKEN_H

typedef enum
{
  // ----- Literais -----
  TOKEN_INT,
  TOKEN_FLOAT,
  TOKEN_STRING,
  TOKEN_IDENTIFIER,

  // ----- Palavras-chave: estrutura -----
  TOKEN_FUNC,
  TOKEN_END,
  TOKEN_RETURN,
  TOKEN_CONST,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_NEW,
  TOKEN_THIS,

  // ----- Palavras-chave: módulos -----
  TOKEN_NAMESPACE,
  TOKEN_IMPORT,
  TOKEN_FROM,

  // ----- Palavras-chave: OOP -----
  TOKEN_CLASS,
  TOKEN_INTERFACE,
  TOKEN_ENUM,
  TOKEN_TRAIT,
  TOKEN_USE,
  TOKEN_IMPLEMENTS,
  TOKEN_EXTENDS,
  TOKEN_CONSTRUCT,

  // ----- Palavras-chave: modificadores -----
  TOKEN_PUBLIC,
  TOKEN_PRIVATE,
  TOKEN_PROTECTED,
  TOKEN_READONLY,
  TOKEN_STATIC,

  // ----- Palavras-chave: loops -----
  TOKEN_FOREACH,
  TOKEN_AS,
  TOKEN_FOR,
  TOKEN_IN,
  TOKEN_WHILE,

  // ----- Palavras-chave: componentes/rotas -----
  TOKEN_PAGE,
  TOKEN_COMPONENT,
  TOKEN_STATE,
  TOKEN_ACTION,
  TOKEN_VIEW,
  TOKEN_ROUTE,
  TOKEN_GROUP,
  TOKEN_MIDDLEWARE,
  TOKEN_APP,

  // ----- Tipos primitivos -----
  TOKEN_TYPE_INT,
  TOKEN_TYPE_FLOAT,
  TOKEN_TYPE_STRING,
  TOKEN_TYPE_BOOL,
  TOKEN_TYPE_OBJECT,
  TOKEN_TYPE_ARRAY,
  TOKEN_TYPE_NUMERIC,

  // ----- Símbolos: um caractere -----
  TOKEN_COLON,
  TOKEN_EQUALS,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  TOKEN_LBRACKET,
  TOKEN_RBRACKET,
  TOKEN_LESS,
  TOKEN_GREATER,
  TOKEN_PIPE,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_BANG,
  TOKEN_QUESTION,

  // ----- Símbolos: dois caracteres -----
  TOKEN_ARROW,       // ->
  TOKEN_FAT_ARROW,   // =>
  TOKEN_EQ_EQ,       // ==
  TOKEN_BANG_EQ,     // !=
  TOKEN_LESS_EQ,     // <=
  TOKEN_GREATER_EQ,  // >=
  TOKEN_AND,         // &&
  TOKEN_OR,          // ||
  TOKEN_PLUS_PLUS,   // ++
  TOKEN_MINUS_MINUS, // --

  // ----- Controle -----
  TOKEN_NEWLINE,
  TOKEN_EOF,
  TOKEN_ERROR
} TokenType;

typedef struct
{
  TokenType type;
  char *lexeme;
  int line;
  int column;
} Token;

const char *token_type_name(TokenType type);

#endif