#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <tree_sitter/parser.h>

#define MAX_INDENT_DEPTH 256
#define TAB_WIDTH 2

enum TokenType {
  INDENT,
  DEDENT,
  NEWLINE,
  PSEUDO_CLASS,
  SELECTOR_PLUS,
  SELECTOR_TILDE,
};

typedef struct {
  uint16_t indents[MAX_INDENT_DEPTH];
  uint16_t depth;
  uint16_t pending_dedents;
} Scanner;

static void advance(TSLexer *lexer) {
  lexer->advance(lexer, false);
}

static void skip(TSLexer *lexer) {
  lexer->advance(lexer, true);
}

static bool is_newline(int32_t character) {
  return character == '\n' || character == '\r';
}

static void consume_newline(TSLexer *lexer) {
  int32_t first = lexer->lookahead;
  advance(lexer);
  if (first == '\r' && lexer->lookahead == '\n') {
    advance(lexer);
  }
}

static uint16_t scan_indentation(TSLexer *lexer, bool skip_characters) {
  uint16_t indent = 0;

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    uint16_t width = lexer->lookahead == '\t' ? TAB_WIDTH : 1;
    indent = indent > UINT16_MAX - width ? UINT16_MAX : indent + width;
    lexer->advance(lexer, skip_characters);
  }

  return indent;
}

static bool is_identifier_start(int32_t character) {
  return character == '-' || character == '_' ||
         (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z');
}

static bool is_identifier_continue(int32_t character) {
  return is_identifier_start(character) ||
         (character >= '0' && character <= '9');
}

static bool scan_pseudo_class(TSLexer *lexer) {
  if (lexer->lookahead != ':') {
    return false;
  }

  advance(lexer);

  // Leave pseudo-elements such as ::before to the regular lexer.
  if (lexer->lookahead == ':' || !is_identifier_start(lexer->lookahead)) {
    return false;
  }

  advance(lexer);
  while (is_identifier_continue(lexer->lookahead)) {
    advance(lexer);
  }

  if (lexer->lookahead != '(') {
    lexer->result_symbol = PSEUDO_CLASS;
    return true;
  }

  uint16_t depth = 0;
  int32_t quote = 0;
  bool escaped = false;

  while (!lexer->eof(lexer)) {
    int32_t character = lexer->lookahead;
    advance(lexer);

    if (escaped) {
      escaped = false;
      continue;
    }

    if (character == '\\') {
      escaped = true;
      continue;
    }

    if (quote != 0) {
      if (character == quote) {
        quote = 0;
      }
      continue;
    }

    if (character == '\'' || character == '"') {
      quote = character;
    } else if (character == '(') {
      depth++;
    } else if (character == ')') {
      depth--;
      if (depth == 0) {
        lexer->result_symbol = PSEUDO_CLASS;
        return true;
      }
    }
  }

  return false;
}

static uint16_t current_indent(const Scanner *scanner);

static bool scan_selector_combinator(Scanner *scanner, TSLexer *lexer, int32_t operator, enum TokenType symbol) {
  if (lexer->lookahead != operator) {
    return false;
  }

  advance(lexer);
  lexer->mark_end(lexer);

  if (lexer->lookahead == '=') {
    return false;
  }

  bool has_target = false;
  bool opens_brace_block = false;

  while (!lexer->eof(lexer) && !is_newline(lexer->lookahead)) {
    if (lexer->lookahead == '{') {
      opens_brace_block = true;
    } else if (lexer->lookahead != ' ' && lexer->lookahead != '\t' && lexer->lookahead != '\f') {
      has_target = true;
    }
    advance(lexer);
  }

  if (!has_target) {
    return false;
  }

  if (opens_brace_block) {
    lexer->result_symbol = symbol;
    return true;
  }

  // Prefer the selector token when the line opens an indented rule. In all
  // other contexts the regular lexer remains free to interpret + or ~ as an
  // additive/comparison operator or as an ordinary selector combinator.
  while (is_newline(lexer->lookahead)) {
    consume_newline(lexer);
    uint16_t indent = scan_indentation(lexer, false);

    if (is_newline(lexer->lookahead)) {
      continue;
    }

    if (indent > current_indent(scanner)) {
      lexer->result_symbol = symbol;
      return true;
    }
    break;
  }

  return false;
}

static uint16_t current_indent(const Scanner *scanner) {
  if (scanner->depth == 0) {
    return 0;
  }

  return scanner->indents[scanner->depth - 1];
}

void *tree_sitter_stylus_external_scanner_create(void) {
  Scanner *scanner = calloc(1, sizeof(Scanner));
  if (scanner == NULL) {
    return NULL;
  }

  scanner->indents[0] = 0;
  scanner->depth = 1;
  scanner->pending_dedents = 0;
  return scanner;
}

void tree_sitter_stylus_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_stylus_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = payload;
  unsigned size = 0;
  uint16_t depth = scanner->depth > MAX_INDENT_DEPTH ? MAX_INDENT_DEPTH : scanner->depth;

  buffer[size++] = (char)(depth & 0xff);
  buffer[size++] = (char)(depth >> 8);
  buffer[size++] = (char)(scanner->pending_dedents & 0xff);
  buffer[size++] = (char)(scanner->pending_dedents >> 8);

  for (uint16_t i = 0; i < depth && size + 1 < TREE_SITTER_SERIALIZATION_BUFFER_SIZE; i++) {
    buffer[size++] = (char)(scanner->indents[i] & 0xff);
    buffer[size++] = (char)(scanner->indents[i] >> 8);
  }

  return size;
}

void tree_sitter_stylus_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = payload;
  scanner->indents[0] = 0;
  scanner->depth = 1;
  scanner->pending_dedents = 0;

  if (length < 4) {
    return;
  }

  scanner->depth = (uint8_t)buffer[0] | ((uint8_t)buffer[1] << 8);
  scanner->pending_dedents = (uint8_t)buffer[2] | ((uint8_t)buffer[3] << 8);

  if (scanner->depth == 0 || scanner->depth > MAX_INDENT_DEPTH) {
    scanner->depth = 1;
    scanner->indents[0] = 0;
    scanner->pending_dedents = 0;
    return;
  }

  unsigned index = 4;
  for (uint16_t i = 0; i < scanner->depth; i++) {
    if (index + 1 >= length) {
      scanner->depth = i == 0 ? 1 : i;
      break;
    }

    scanner->indents[i] = (uint8_t)buffer[index] | ((uint8_t)buffer[index + 1] << 8);
    index += 2;
  }
}

static bool scan_newline(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (scanner->pending_dedents > 0 && valid_symbols[DEDENT]) {
    scanner->pending_dedents--;
    lexer->result_symbol = DEDENT;
    return true;
  }

  if (lexer->eof(lexer)) {
    if (scanner->depth > 1 && valid_symbols[DEDENT]) {
      scanner->depth--;
      lexer->result_symbol = DEDENT;
      return true;
    }

    return false;
  }

  if (!is_newline(lexer->lookahead)) {
    return false;
  }

  uint16_t indent = 0;

  while (is_newline(lexer->lookahead)) {
    consume_newline(lexer);
    indent = scan_indentation(lexer, true);

    if (is_newline(lexer->lookahead)) {
      indent = 0;
      continue;
    }

    break;
  }

  uint16_t current = current_indent(scanner);

  if (indent > current) {
    if (!valid_symbols[INDENT]) {
      return false;
    }

    if (scanner->depth < MAX_INDENT_DEPTH) {
      scanner->indents[scanner->depth++] = indent;
    }

    lexer->result_symbol = INDENT;
    return true;
  }

  if (indent < current) {
    if (!valid_symbols[DEDENT]) {
      return false;
    }

    while (scanner->depth > 1 && indent < current_indent(scanner)) {
      scanner->depth--;
      scanner->pending_dedents++;
    }

    if (scanner->pending_dedents > 0) {
      scanner->pending_dedents--;
      lexer->result_symbol = DEDENT;
      return true;
    }

    return false;
  }

  if (valid_symbols[NEWLINE]) {
    lexer->result_symbol = NEWLINE;
    return true;
  }

  return false;
}

bool tree_sitter_stylus_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  Scanner *scanner = payload;

  if (valid_symbols[PSEUDO_CLASS] || valid_symbols[SELECTOR_PLUS] || valid_symbols[SELECTOR_TILDE]) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\f') {
      skip(lexer);
    }
  }

  if (valid_symbols[PSEUDO_CLASS] && lexer->lookahead == ':') {
    return scan_pseudo_class(lexer);
  }

  if (valid_symbols[SELECTOR_PLUS] && lexer->lookahead == '+') {
    return scan_selector_combinator(scanner, lexer, '+', SELECTOR_PLUS);
  }

  if (valid_symbols[SELECTOR_TILDE] && lexer->lookahead == '~') {
    return scan_selector_combinator(scanner, lexer, '~', SELECTOR_TILDE);
  }

  return scan_newline(scanner, lexer, valid_symbols);
}
