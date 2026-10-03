#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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
  ID_NAME,
  COLOR_VALUE,
  DECLARATION_COLON,
};

typedef struct {
  uint16_t indents[MAX_INDENT_DEPTH];
  uint16_t depth;
  uint16_t pending_dedents;
  uint32_t content_column;
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

static uint16_t current_indent(const Scanner *scanner);

static bool scan_comment(TSLexer *lexer) {
  if (lexer->lookahead != '/') return false;
  advance(lexer);
  if (lexer->lookahead == '/') {
    while (!lexer->eof(lexer) && !is_newline(lexer->lookahead)) advance(lexer);
  } else if (lexer->lookahead == '*') {
    advance(lexer);
    while (!lexer->eof(lexer)) {
      if (lexer->lookahead == '*') {
        advance(lexer);
        if (lexer->lookahead == '/') {
          advance(lexer);
          break;
        }
      } else {
        advance(lexer);
      }
    }
    scan_indentation(lexer, false);
  } else {
    return false;
  }
  return true;
}

static bool line_opens_block(Scanner *scanner, TSLexer *lexer, bool selector_list) {
  bool trailing_comma = false;
  int32_t quote = 0;
  bool escaped = false;
  while (!lexer->eof(lexer) && !is_newline(lexer->lookahead)) {
    if (quote != 0) {
      int32_t character = lexer->lookahead;
      advance(lexer);
      if (escaped) escaped = false;
      else if (character == '\\') escaped = true;
      else if (character == quote) quote = 0;
      continue;
    }
    if (lexer->lookahead == '\'' || lexer->lookahead == '"') {
      quote = lexer->lookahead;
      trailing_comma = false;
      advance(lexer);
      continue;
    }
    if (lexer->lookahead == '/' && scan_comment(lexer)) continue;
    if (lexer->lookahead == '{') return true;
    if (lexer->lookahead != ' ' && lexer->lookahead != '\t' && lexer->lookahead != '\f') {
      trailing_comma = lexer->lookahead == ',';
    }
    advance(lexer);
  }
  if (selector_list && trailing_comma) return true;
  while (is_newline(lexer->lookahead)) {
    consume_newline(lexer);
    uint16_t indent = scan_indentation(lexer, false);
    while (lexer->lookahead == '/' && scan_comment(lexer)) {}
    if (is_newline(lexer->lookahead)) continue;
    if (lexer->eof(lexer)) return false;
    return indent > current_indent(scanner);
  }
  return false;
}

static bool scan_hash_name(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (lexer->lookahead != '#') return false;
  bool starts_line = lexer->get_column(lexer) == scanner->content_column;
  advance(lexer);
  bool identifier = is_identifier_start(lexer->lookahead);
  unsigned length = 0;
  bool hex = true;
  while (is_identifier_continue(lexer->lookahead)) {
    int32_t character = lexer->lookahead;
    hex = hex && ((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f') ||
                  (character >= 'A' && character <= 'F'));
    length++;
    advance(lexer);
  }
  // Use the complete token and parser context, never a color prefix of an ID.
  lexer->mark_end(lexer);
  if (hex && (length == 3 || length == 4 || length == 6 || length == 8) &&
      valid_symbols[COLOR_VALUE]) {
    lexer->result_symbol = identifier && valid_symbols[ID_NAME] &&
      (starts_line || line_opens_block(scanner, lexer, false))
      ? ID_NAME : COLOR_VALUE;
    return true;
  }
  if (identifier && valid_symbols[ID_NAME]) {
    lexer->result_symbol = ID_NAME;
    return true;
  }
  return false;
}

static bool finish_pseudo_class(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols, bool known_pseudo) {
  lexer->mark_end(lexer);
  // Compact declarations such as content:counter(item) share their prefix
  // with nested pseudo-selectors. Match Stylus's known-name and trailing-comma
  // selector rules; other names must open a block.
  if (valid_symbols[DECLARATION_COLON] && !known_pseudo && !line_opens_block(scanner, lexer, true)) return false;
  lexer->result_symbol = PSEUDO_CLASS;
  return true;
}

static bool scan_pseudo_class(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (lexer->lookahead != ':') {
    return false;
  }

  advance(lexer);

  // Leave pseudo-elements such as ::before to the regular lexer.
  if (lexer->lookahead == ':' || !is_identifier_start(lexer->lookahead)) {
    return false;
  }

  // Stylus recognizes these names as selectors even before a shared block.
  static const char *const pseudo_names[] = {
    "is", "has", "where", "not", "dir", "lang", "any-link", "link", "visited",
    "local-link", "target", "scope", "hover", "active", "focus", "drop", "current",
    "past", "future", "enabled", "disabled", "read-only", "read-write",
    "placeholder-shown", "checked", "indeterminate", "valid", "invalid", "in-range",
    "out-of-range", "required", "optional", "user-error", "root", "empty", "blank",
    "nth-child", "nth-last-child", "first-child", "last-child", "only-child",
    "nth-of-type", "nth-last-of-type", "first-of-type", "last-of-type", "only-of-type",
    "nth-match", "nth-last-match", "nth-column", "nth-last-column", "first-line",
    "first-letter", "before", "after", "selection",
  };
  char name[32] = {0};
  unsigned name_length = 0;
  while (is_identifier_continue(lexer->lookahead)) {
    if (name_length < sizeof(name) - 1) name[name_length++] = (char)lexer->lookahead;
    advance(lexer);
  }
  bool known_pseudo = false;
  for (unsigned i = 0; i < sizeof(pseudo_names) / sizeof(pseudo_names[0]); i++) {
    if (strcmp(name, pseudo_names[i]) == 0) {
      known_pseudo = true;
      break;
    }
  }

  if (lexer->lookahead != '(') {
    return finish_pseudo_class(scanner, lexer, valid_symbols, known_pseudo);
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
        return finish_pseudo_class(scanner, lexer, valid_symbols, known_pseudo);
      }
    }
  }

  return false;
}

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
  for (unsigned i = 0; i < 4; i++) {
    buffer[size++] = (char)(scanner->content_column >> (i * 8));
  }

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
  scanner->content_column = 0;

  if (length < 8) {
    return;
  }

  scanner->depth = (uint8_t)buffer[0] | ((uint8_t)buffer[1] << 8);
  scanner->pending_dedents = (uint8_t)buffer[2] | ((uint8_t)buffer[3] << 8);
  for (unsigned i = 0; i < 4; i++) {
    scanner->content_column |= (uint32_t)(uint8_t)buffer[4 + i] << (i * 8);
  }

  if (scanner->depth == 0 || scanner->depth > MAX_INDENT_DEPTH) {
    scanner->depth = 1;
    scanner->indents[0] = 0;
    scanner->pending_dedents = 0;
    return;
  }

  unsigned index = 8;
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
  bool marked_end = false;

  while (is_newline(lexer->lookahead)) {
    consume_newline(lexer);
    indent = scan_indentation(lexer, true);

    // Look through comment-only lines without consuming their comment tokens.
    while (lexer->lookahead == '/') {
      if (!marked_end) {
        lexer->mark_end(lexer);
        marked_end = true;
      }
      if (!scan_comment(lexer)) break;
    }

    if (is_newline(lexer->lookahead)) {
      indent = 0;
      continue;
    }

    break;
  }

  uint16_t current = current_indent(scanner);
  uint32_t content_column = lexer->get_column(lexer);
  if (lexer->eof(lexer)) indent = 0;

  if (indent > current) {
    if (!valid_symbols[INDENT]) {
      if (scanner->depth == 1 && valid_symbols[NEWLINE]) {
        scanner->indents[0] = indent;
        scanner->content_column = content_column;
        lexer->result_symbol = NEWLINE;
        return true;
      }
      return false;
    }

    if (scanner->depth < MAX_INDENT_DEPTH) {
      scanner->indents[scanner->depth++] = indent;
    }

    scanner->content_column = content_column;
    lexer->result_symbol = INDENT;
    return true;
  }

  if (indent < current) {
    if (scanner->depth == 1 && valid_symbols[NEWLINE]) {
      scanner->indents[0] = indent;
      scanner->content_column = content_column;
      lexer->result_symbol = NEWLINE;
      return true;
    }
    if (!valid_symbols[DEDENT]) {
      return false;
    }

    while (scanner->depth > 1 && indent < current_indent(scanner)) {
      scanner->depth--;
      scanner->pending_dedents++;
    }
    if (indent < scanner->indents[0]) {
      scanner->indents[0] = indent;
    }

    if (scanner->pending_dedents > 0) {
      scanner->pending_dedents--;
      scanner->content_column = content_column;
      lexer->result_symbol = DEDENT;
      return true;
    }

    return false;
  }

  if (valid_symbols[NEWLINE]) {
    scanner->content_column = content_column;
    lexer->result_symbol = NEWLINE;
    return true;
  }

  return false;
}

bool tree_sitter_stylus_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  Scanner *scanner = payload;

  if (valid_symbols[PSEUDO_CLASS] || valid_symbols[SELECTOR_PLUS] || valid_symbols[SELECTOR_TILDE] ||
      valid_symbols[ID_NAME] || valid_symbols[COLOR_VALUE]) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\f') {
      skip(lexer);
    }
  }
  while (lexer->lookahead == ';') {
    skip(lexer);
  }

  if (valid_symbols[PSEUDO_CLASS] && lexer->lookahead == ':') {
    return scan_pseudo_class(scanner, lexer, valid_symbols);
  }

  if ((valid_symbols[ID_NAME] || valid_symbols[COLOR_VALUE]) && lexer->lookahead == '#') {
    return scan_hash_name(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[SELECTOR_PLUS] && lexer->lookahead == '+') {
    return scan_selector_combinator(scanner, lexer, '+', SELECTOR_PLUS);
  }

  if (valid_symbols[SELECTOR_TILDE] && lexer->lookahead == '~') {
    return scan_selector_combinator(scanner, lexer, '~', SELECTOR_TILDE);
  }

  return scan_newline(scanner, lexer, valid_symbols);
}
