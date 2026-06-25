#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_at_end(Lexer *lexer)
{
  return lexer->source[lexer->current] == '\0';
}
static char peek(Lexer *lexer)
{
  return lexer->source[lexer->current];
}
static char peek_next(Lexer *lexer)
{
  if (is_at_end(lexer))
    return '\0';
  return lexer->source[lexer->current + 1];
}
static char advance(Lexer *lexer)
{
  char c = lexer->source[lexer->current];
  lexer->current++;
  lexer->column++;
  return c;
}
// Consome o próximo char SE ele for o esperado (para símbolos de 2 caracteres)
static int match(Lexer *lexer, char expected)
{
  if (is_at_end(lexer))
    return 0;
  if (lexer->source[lexer->current] != expected)
    return 0;
  lexer->current++;
  lexer->column++;
  return 1;
}

static char *copy_lexeme(Lexer *lexer)
{
  int length = lexer->current - lexer->start;
  char *text = (char *)malloc(length + 1);
  memcpy(text, lexer->source + lexer->start, length);
  text[length] = '\0';
  return text;
}
static Token make_token(Lexer *lexer, TokenType type)
{
  Token token;
  token.type = type;
  token.lexeme = copy_lexeme(lexer);
  token.line = lexer->line;
  token.column = lexer->column;
  return token;
}
static Token error_token(Lexer *lexer, const char *message)
{
  Token token;
  token.type = TOKEN_ERROR;
  token.lexeme = strdup(message);
  token.line = lexer->line;
  token.column = lexer->column;
  return token;
}

// Pula espaços/tabs/CR, comentários // e diretivas # (ex: #!strict). NÃO pula '\n'.
static void skip_whitespace(Lexer *lexer)
{
  for (;;)
  {
    char c = peek(lexer);
    switch (c)
    {
    case ' ':
    case '\t':
    case '\r':
      advance(lexer);
      break;
    case '/':
      if (peek_next(lexer) == '/')
      {
        while (peek(lexer) != '\n' && !is_at_end(lexer))
          advance(lexer);
      }
      else
      {
        return; // é divisão, não comentário
      }
      break;
    case '#':
      // diretiva (tratada na fase de tipos); por ora ignoramos a linha
      while (peek(lexer) != '\n' && !is_at_end(lexer))
        advance(lexer);
      break;
    default:
      return;
    }
  }
}

static int is_alpha(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_digit(char c) { return isdigit((unsigned char)c); }

static TokenType identifier_type(Lexer *lexer)
{
  int length = lexer->current - lexer->start;
  const char *text = lexer->source + lexer->start;

  struct
  {
    const char *word;
    TokenType type;
  } keywords[] = {
      {"func", TOKEN_FUNC},
      {"end", TOKEN_END},
      {"return", TOKEN_RETURN},
      {"const", TOKEN_CONST},
      {"if", TOKEN_IF},
      {"else", TOKEN_ELSE},
      {"true", TOKEN_TRUE},
      {"false", TOKEN_FALSE},
      {"new", TOKEN_NEW},
      {"this", TOKEN_THIS},
      {"namespace", TOKEN_NAMESPACE},
      {"import", TOKEN_IMPORT},
      {"from", TOKEN_FROM},
      {"class", TOKEN_CLASS},
      {"interface", TOKEN_INTERFACE},
      {"enum", TOKEN_ENUM},
      {"trait", TOKEN_TRAIT},
      {"use", TOKEN_USE},
      {"implements", TOKEN_IMPLEMENTS},
      {"extends", TOKEN_EXTENDS},
      {"construct", TOKEN_CONSTRUCT},
      {"public", TOKEN_PUBLIC},
      {"private", TOKEN_PRIVATE},
      {"protected", TOKEN_PROTECTED},
      {"readonly", TOKEN_READONLY},
      {"static", TOKEN_STATIC},
      {"foreach", TOKEN_FOREACH},
      {"as", TOKEN_AS},
      {"for", TOKEN_FOR},
      {"in", TOKEN_IN},
      {"while", TOKEN_WHILE},
      {"page", TOKEN_PAGE},
      {"component", TOKEN_COMPONENT},
      {"state", TOKEN_STATE},
      {"action", TOKEN_ACTION},
      {"view", TOKEN_VIEW},
      {"route", TOKEN_ROUTE},
      {"group", TOKEN_GROUP},
      {"middleware", TOKEN_MIDDLEWARE},
      {"app", TOKEN_APP},
      {"int", TOKEN_TYPE_INT},
      {"float", TOKEN_TYPE_FLOAT},
      {"string", TOKEN_TYPE_STRING},
      {"bool", TOKEN_TYPE_BOOL},
      {"object", TOKEN_TYPE_OBJECT},
      {"array", TOKEN_TYPE_ARRAY},
      {"numeric", TOKEN_TYPE_NUMERIC},
  };
  int count = sizeof(keywords) / sizeof(keywords[0]);
  for (int i = 0; i < count; i++)
  {
    if ((int)strlen(keywords[i].word) == length &&
        memcmp(text, keywords[i].word, length) == 0)
    {
      return keywords[i].type;
    }
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer *lexer)
{
  while (is_alpha(peek(lexer)) || is_digit(peek(lexer)))
    advance(lexer);
  return make_token(lexer, identifier_type(lexer));
}

static Token number(Lexer *lexer)
{
  while (is_digit(peek(lexer)))
    advance(lexer);
  int is_float = 0;
  if (peek(lexer) == '.' && is_digit(peek_next(lexer)))
  {
    is_float = 1;
    advance(lexer);
    while (is_digit(peek(lexer)))
      advance(lexer);
  }
  return make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INT);
}

static Token string(Lexer *lexer)
{
  while (peek(lexer) != '"' && !is_at_end(lexer))
  {
    if (peek(lexer) == '\n')
      return error_token(lexer, "String nao terminada");
    advance(lexer);
  }
  if (is_at_end(lexer))
    return error_token(lexer, "String nao terminada");
  advance(lexer); // aspas final
  return make_token(lexer, TOKEN_STRING);
}

void lexer_init(Lexer *lexer, const char *source)
{
  lexer->source = source;
  lexer->start = 0;
  lexer->current = 0;
  lexer->line = 1;
  lexer->column = 1;
}

Token lexer_next_token(Lexer *lexer)
{
  skip_whitespace(lexer);
  lexer->start = lexer->current;

  if (is_at_end(lexer))
    return make_token(lexer, TOKEN_EOF);

  char c = advance(lexer);

  if (c == '\n')
  {
    Token token = make_token(lexer, TOKEN_NEWLINE);
    lexer->line++;
    lexer->column = 1;
    return token;
  }

  if (is_alpha(c))
    return identifier(lexer);
  if (is_digit(c))
    return number(lexer);

  switch (c)
  {
  case ':':
    return make_token(lexer, TOKEN_COLON);
  case '(':
    return make_token(lexer, TOKEN_LPAREN);
  case ')':
    return make_token(lexer, TOKEN_RPAREN);
  case '{':
    return make_token(lexer, TOKEN_LBRACE);
  case '}':
    return make_token(lexer, TOKEN_RBRACE);
  case '[':
    return make_token(lexer, TOKEN_LBRACKET);
  case ']':
    return make_token(lexer, TOKEN_RBRACKET);
  case ',':
    return make_token(lexer, TOKEN_COMMA);
  case '.':
    return make_token(lexer, TOKEN_DOT);
  case '*':
    return make_token(lexer, TOKEN_STAR);
  case '/':
    return make_token(lexer, TOKEN_SLASH);
  case '?':
    return make_token(lexer, TOKEN_QUESTION);
  case '"':
    return string(lexer);

  case '=':
    if (match(lexer, '='))
      return make_token(lexer, TOKEN_EQ_EQ);
    if (match(lexer, '>'))
      return make_token(lexer, TOKEN_FAT_ARROW);
    return make_token(lexer, TOKEN_EQUALS);
  case '!':
    if (match(lexer, '='))
      return make_token(lexer, TOKEN_BANG_EQ);
    return make_token(lexer, TOKEN_BANG);
  case '<':
    if (match(lexer, '='))
      return make_token(lexer, TOKEN_LESS_EQ);
    return make_token(lexer, TOKEN_LESS);
  case '>':
    if (match(lexer, '='))
      return make_token(lexer, TOKEN_GREATER_EQ);
    return make_token(lexer, TOKEN_GREATER);
  case '+':
    if (match(lexer, '+'))
      return make_token(lexer, TOKEN_PLUS_PLUS);
    return make_token(lexer, TOKEN_PLUS);
  case '-':
    if (match(lexer, '>'))
      return make_token(lexer, TOKEN_ARROW);
    if (match(lexer, '-'))
      return make_token(lexer, TOKEN_MINUS_MINUS);
    return make_token(lexer, TOKEN_MINUS);
  case '|':
    if (match(lexer, '|'))
      return make_token(lexer, TOKEN_OR);
    return make_token(lexer, TOKEN_PIPE);
  case '&':
    if (match(lexer, '&'))
      return make_token(lexer, TOKEN_AND);
    return error_token(lexer, "Caractere inesperado: &");
  }

  return error_token(lexer, "Caractere inesperado");
}