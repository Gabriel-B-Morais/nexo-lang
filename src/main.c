#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "parser/parser.h"

static char *read_file(const char *path)
{
  FILE *file = fopen(path, "rb");
  if (!file)
  {
    fprintf(stderr, "Nao foi possivel abrir: %s\n", path);
    return NULL;
  }
  fseek(file, 0L, SEEK_END);
  long size = ftell(file);
  rewind(file);
  char *buffer = (char *)malloc(size + 1);
  if (!buffer)
  {
    fclose(file);
    return NULL;
  }
  size_t read = fread(buffer, 1, size, file);
  buffer[read] = '\0';
  fclose(file);
  return buffer;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("Uso: %s <arquivo.nx>\n", argv[0]);
    return 1;
  }
  char *source = read_file(argv[1]);
  if (!source)
    return 1;

  int had_error = 0;
  Stmt *program = parse(source, &had_error);

  if (had_error)
  {
    fprintf(stderr, "\nParsing falhou com erros.\n");
    free(source);
    return 1;
  }

  ast_print(program);
  free(source);
  return 0;
}