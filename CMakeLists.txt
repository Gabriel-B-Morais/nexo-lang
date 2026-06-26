cmake_minimum_required(VERSION 3.15)
project(nexo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(nexo
  src/main.c
  src/lexer/lexer.c
  src/lexer/token.c
  src/parser/parser.c
  src/parser/ast_print.c
  src/runtime/value.c
  src/runtime/environment.c
  src/interpreter/interpreter.c
)