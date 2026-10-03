const PREC = {
  ASSIGN: 1,
  POSTFIX: 2,
  TERNARY: 3,
  OBJECT: 4,
  NOT: 5,
  LOGICAL: 6,
  TYPE_CHECK: 7,
  EQUALITY: 8,
  IN: 9,
  COMPARE: 10,
  RANGE: 11,
  ADD: 12,
  MULTIPLY: 13,
  POWER: 14,
  DEFINED: 15,
  UNARY: 16,
  CALL: 17,
};

module.exports = grammar({
  name: 'stylus',

  externals: $ => [
    $._indent,
    $._dedent,
    $._newline,
    $.pseudo_class,
    $._selector_plus,
    $._selector_tilde,
    $.id_name,
    $.color_value,
    $._declaration_colon,
  ],

  extras: $ => [
    /[ \t\f;]+/,
    /\\\r?\n[ \t]*/,
    $.comment,
  ],

  word: $ => $.identifier,

  conflicts: $ => [
    // Statement termination is indentation-sensitive, so the final statement
    // in a block can be valid both with and without an explicit newline.
    [$._root_statement_with_sep, $._root_statement],
    [$._block_statement_with_sep, $._block_statement],

    // A bare Stylus line can represent a selector, declaration, or
    // expression. Keep these conflicts explicit and grouped by domain.
    [$.selector, $.variable_name],
    [$.nested_selector, $.variable_name],
    [$.interpolation, $.selector_interpolation],
    [$._property_interpolation, $.selector_interpolation],
    [$.interpolated_property_name],
    [$.declaration, $.nested_selector],
    [$.declaration, $.selector],
    [$.declaration, $.variable_name],
    [$.declaration, $.selector, $.variable_name],
    [$.declaration, $.nested_selector, $.variable_name],
    [$.nested_selector, $.css_declaration],
    [$.parameter, $._value],
    [$.for_statement, $.variable_name],

    // Media and feature queries intentionally reuse the expression grammar.
    [$.media_statement, $.media_query],
    [$.supports_statement, $.media_query],
    [$.media_feature, $.variable_name],
    [$.feature_query, $.media_feature],
    [$.feature_query, $.variable_name],
    [$._declaration_value, $.feature_query],
    [$.expression, $._css_declaration_value],
  ],

  rules: {
    stylesheet: $ => seq(
      repeat(choice(
        $._root_statement_with_sep,
        $._newline,
      )),
      optional($._root_statement),
    ),

    _root_statement_with_sep: $ => choice(
      $._root_block_statement,
      seq($._root_line_statement, $._newline),
    ),

    _block_statement_with_sep: $ => choice(
      $._block_block_statement,
      seq($._block_line_statement, $._newline),
    ),

    _root_statement: $ => choice(
      $._root_block_statement,
      $._root_line_statement,
    ),

    _root_line_statement: $ => choice(
      $.import_statement,
      $.assignment,
      $.extend_statement,
      $.call_expression,
      $.generic_at_rule_declaration,
      $.postfix_statement,
    ),

    _root_block_statement: $ => choice(
      $.rule_set,
      alias($.root_function_statement, $.function_statement),
      $.if_statement,
      $.unless_statement,
      $.each_statement,
      $.for_statement,
      $.while_statement,
      $.media_statement,
      $.supports_statement,
      $.font_face_statement,
      $.keyframes_statement,
      $.block_assignment,
      $.generic_at_rule,
      $.css_literal_statement,
    ),

    _block_statement: $ => choice(
      $._block_block_statement,
      $._block_line_statement,
    ),

    _block_line_statement: $ => choice(
      $.import_statement,
      $.assignment,
      $.group_declaration,
      $.declaration,
      $.extend_statement,
      $.return_statement,
      $.expression_statement,
      $.generic_at_rule_declaration,
      $.postfix_statement,
    ),

    _block_block_statement: $ => choice(
      $.nested_declaration,
      alias($.nested_rule_set, $.rule_set),
      $.function_statement,
      $.if_statement,
      $.unless_statement,
      $.each_statement,
      $.for_statement,
      $.while_statement,
      $.media_statement,
      $.supports_statement,
      $.font_face_statement,
      $.keyframes_statement,
      $.block_assignment,
      $.generic_at_rule,
      $.css_literal_statement,
    ),

    block: $ => seq(
      $._indent,
      repeat(choice(
        $._block_statement_with_sep,
        $._newline,
      )),
      optional($._block_statement),
      $._dedent,
    ),

    root_function_statement: $ => functionDefinition($),

    import_statement: $ => seq(
      choice('@import', 'import', '@require', 'require'),
      field('source', $.string_value),
    ),

    assignment: $ => prec.dynamic(10, prec.right(PREC.ASSIGN, seq(
      field('left', $.variable_name),
      field('operator', choice('=', '?=', ':=', '+=', '-=', '*=', '/=', '%=')),
      field('right', $.expression),
      repeat(seq(optional(','), field('right', $.expression))),
    ))),

    block_assignment: $ => prec.dynamic(25, seq(
      field('left', $.variable_name),
      field('operator', '='),
      field('right', $.block),
    )),

    declaration: $ => prec.dynamic(5, prec.right(choice(
      declarationRule($, $.interpolated_property_name),
      declarationRule($, alias($.identifier, $.property_name)),
      declarationRule($, alias($.custom_property_name, $.property_name)),
      declarationRule($, alias($.nested_property_name, $.property_name), false),
    ))),

    group_declaration: $ => prec.dynamic(15, prec.right(seq(
      field('property', alias($.nested_property_name, $.property_name)),
      choice(':', alias($._declaration_colon, ':')),
      $._declaration_values,
    ))),

    _declaration_values: $ => declarationValues($, $._declaration_value),

    _declaration_value: $ => choice(prec(1, $.call_expression), $.expression),

    nested_declaration: $ => prec.dynamic(-3, prec(-1, seq(
      field('property', alias($.nested_property_name, $.property_name)),
      $.block,
    ))),

    function_statement: $ => functionDefinition($),

    if_statement: $ => seq(
      'if',
      field('condition', $.expression),
      $.block,
      repeat($.else_if_clause),
      optional($.else_clause),
    ),

    unless_statement: $ => seq(
      'unless',
      field('condition', $.expression),
      $.block,
      optional($.else_clause),
    ),

    else_if_clause: $ => seq(
      'else',
      'if',
      field('condition', $.expression),
      $.block,
    ),

    else_clause: $ => seq(
      'else',
      $.block,
    ),

    each_statement: $ => seq(
      'each',
      field('item', $.identifier),
      optional(seq(',', field('index', $.identifier))),
      'in',
      field('iterable', $.expression),
      $.block,
    ),

    for_statement: $ => choice(
      prec.dynamic(1, seq(
        'for',
        field('item', $.identifier),
        optional(seq(',', field('index', $.identifier))),
        'in',
        field('iterable', $.expression),
        repeat(seq(optional(','), field('iterable', $.expression))),
        $.block,
      )),
      seq(
        'for',
        field('condition', $.expression),
        $.block,
      ),
    ),

    while_statement: $ => seq(
      'while',
      field('condition', $.expression),
      $.block,
    ),

    postfix_statement: $ => prec.dynamic(30, prec.right(PREC.POSTFIX, seq(
      field('body', choice(
        $.import_statement,
        $.assignment,
        $.extend_statement,
        $.return_statement,
        $.declaration,
        $.call_expression,
        $.expression_statement,
      )),
      repeat1(choice(
        $.postfix_condition_clause,
        $.postfix_for_clause,
      )),
    ))),

    postfix_condition_clause: $ => seq(
      field('operator', choice('if', 'unless')),
      field('condition', $.expression),
    ),

    postfix_for_clause: $ => seq(
      'for',
      field('item', $.identifier),
      optional(seq(',', field('index', $.identifier))),
      'in',
      field('iterable', $.expression),
      repeat(seq(optional(','), field('iterable', $.expression))),
    ),

    media_statement: $ => queriedBlock($, choice('@media', 'media')),

    supports_statement: $ => queriedBlock($, choice('@supports', 'supports')),

    font_face_statement: $ => seq(
      field('keyword', alias(token(choice('@font-face', 'font-face')), $.at_keyword)),
      $.block,
    ),

    keyframes_statement: $ => seq(
      choice('@keyframes', 'keyframes', alias(token(/@-(webkit|moz|o|ms)-keyframes/), $.at_keyword)),
      field('name', choice(
        alias($.identifier, $.keyframes_name),
        $.interpolation,
      )),
      choice($.block, $.keyframes_block),
    ),

    css_literal_statement: $ => prec.dynamic(30, seq(
      '@css',
      $.css_declaration_block,
    )),

    generic_at_rule: $ => prec.dynamic(-5, seq(
      field('name', $.at_keyword),
      optional(field('prelude', $.at_rule_prelude)),
      choice($.block, $.css_declaration_block),
    )),

    generic_at_rule_declaration: $ => prec.dynamic(-10, seq(
      field('name', $.at_keyword),
      field('prelude', $.at_rule_prelude),
    )),

    at_keyword: $ => token(/@[a-zA-Z_-][\w-]*/),

    at_rule_prelude: $ => token(prec(-1, /[^{}\s][^{}\n]*/)),

    extend_statement: $ => seq(
      choice('@extend', '@extends', 'extend', 'extends'),
      field('target', $.extend_target_list),
    ),

    extend_target_list: $ => sep1($.extend_target, ','),

    extend_target: $ => seq(
      field('selector', $.nested_selector),
      optional(field('modifier', $.optional_modifier)),
    ),

    expression_statement: $ => prec.dynamic(-10, $.expression),

    optional_modifier: $ => token(seq('!', 'optional')),

    rule_set: $ => ruleSet($, $.selector_list),

    nested_rule_set: $ => ruleSet($, $.nested_selector_list),

    selector_list: $ => sep1($.selector, $._selector_separator),
    nested_selector_list: $ => sep1($.nested_selector, $._selector_separator),

    _selector_separator: $ => choice(
      seq(',', repeat($._newline)),
      repeat1($._newline),
    ),

    selector: $ => selectorSequence($),

    nested_selector: $ => prec.dynamic(2, selectorSequence($)),

    combinator_selector: $ => prec.dynamic(20, prec(PREC.UNARY + 1, seq(
      $.combinator,
      choice(
        alias($.identifier, $.tag_name),
        $.class_name,
        $.id_name,
        $.pseudo_element,
        $.pseudo_class,
        $.attribute_selector,
        $.nesting_selector_with_suffix,
        $.nesting_selector,
        $.universal_selector,
        $.selector_interpolation,
        $.deep_combinator,
      ),
    ))),

    class_name: $ => token(seq('.', /-?[_a-zA-Z][\w-]*/)),
    pseudo_element: $ => token(seq('::', /-?[_a-zA-Z][\w-]*/)),
    nesting_selector_with_suffix: $ => seq(
      $.nesting_selector,
      repeat1($.selector_suffix),
    ),
    selector_suffix: $ => token.immediate(/-{1,2}[_a-zA-Z][\w-]*/),
    universal_selector: $ => '*',
    nesting_selector: $ => '&',
    combinator: $ => choice('>', '+', '~', $._selector_plus, $._selector_tilde),
    deep_combinator: $ => token(choice('/deep/', '>>>')),

    attribute_selector: $ => seq(
      '[',
      field('name', alias($.identifier, $.attribute_name)),
      optional(seq(
        field('operator', choice('=', '~=', '^=', '|=', '*=', '$=')),
        field('value', choice($.string_value, $.identifier, $.number_value)),
      )),
      ']',
    ),

    return_statement: $ => prec.right(seq(
      'return',
      optional(field('value', $.expression)),
    )),

    expression: $ => choice(
      $.ternary_expression,
      $.binary_expression,
      $.defined_expression,
      $.media_prefix_expression,
      $.unary_expression,
      $.not_expression,
      $.css_function_expression,
      $.call_expression,
      $.feature_query,
      $.object_value,
      $.block_literal,
      $.cast_expression,
      $.interpolation,
      $._value,
    ),

    ternary_expression: $ => prec.right(PREC.TERNARY, seq(
      field('condition', $.expression),
      field('operator', '?'),
      field('consequence', $.expression),
      ':',
      field('alternative', $.expression),
    )),

    defined_expression: $ => prec.left(PREC.DEFINED, seq(
      field('value', choice($.variable_name, $.member_expression)),
      field('operator', seq('is', 'defined')),
    )),

    not_expression: $ => prec.right(PREC.NOT, seq(
      field('operator', 'not'),
      field('value', $.expression),
    )),

    cast_expression: $ => prec(PREC.CALL, seq(
      field('value', $.parenthesized_expression),
      field('unit', choice(
        $.unit,
        alias('%', $.unit),
      )),
    )),

    block_literal: $ => prec.dynamic(25, seq(
      '@block',
      $.css_declaration_block,
    )),

    media_prefix_expression: $ => prec.right(PREC.LOGICAL, seq(
      'only',
      $.expression,
    )),

    css_function_expression: $ => seq(
      field('function', alias($.css_function_name, $.function_name)),
      '(',
      optional($._indent),
      optional($.css_function_arguments),
      optional($._dedent),
      ')',
    ),

    css_function_name: $ => token(prec(2, choice(
      'url',
      'calc',
      'clamp',
      'min',
      'max',
      'var',
      'env',
      'rgb',
      'rgba',
      'hsl',
      'hsla',
      'linear-gradient',
      'radial-gradient',
      'repeating-linear-gradient',
      'repeating-radial-gradient',
    ))),

    css_function_arguments: $ => repeat1(choice(
      $.raw_css_function,
      $.expression,
      $.raw_css_value,
      ',',
      '/',
      $._newline,
    )),

    raw_css_function: $ => token(prec(3, /-?[_a-zA-Z][\w.:-]*\([^()\n]*\)/)),

    call_expression: $ => prec.dynamic(20, prec(PREC.CALL, seq(
      field('function', choice(alias($.identifier, $.function_name), alias($.dollar_identifier, $.function_name))),
      field('arguments', $.arguments),
    ))),

    parameters: $ => seq(
      token.immediate('('),
      optional(sep1($.parameter, ',')),
      ')',
    ),

    parameter: $ => prec.right(seq(
      field('name', $.variable_name),
      optional(choice(
        field('rest', '...'),
        seq('=', field('value', $.expression)),
      )),
    )),

    arguments: $ => seq(
      token.immediate('('),
      optional(seq(
        field('argument', $.expression),
        repeat(seq(optional(','), field('argument', $.expression))),
      )),
      ')',
    ),


    feature_query: $ => seq(
      field('name', choice(
        alias($.identifier, $.property_name),
        $.interpolated_property_name,
      )),
      ':',
      field('value', $.expression),
    ),

    media_query: $ => prec.right(seq(
      repeat1(choice(
        $.media_feature,
        $.media_query_keyword,
        $.expression,
        $.raw_css_value,
        ',',
      )),
    )),

    media_query_keyword: $ => choice('only', 'not', 'and', 'or'),

    media_feature: $ => seq(
      '(',
      field('name', choice(
        alias($.identifier, $.property_name),
        $.interpolated_property_name,
      )),
      optional(':'),
      field('value', $.expression),
      ')',
    ),

    interpolated_property_name: $ => prec.dynamic(10, prec.right(1, seq(
      optional(alias($.identifier, $.property_name)),
      alias($._property_interpolation, $.interpolation),
      repeat(choice(
        alias($._property_name_fragment, $.property_name),
        alias($._property_interpolation, $.interpolation),
      )),
    ))),

    _property_interpolation: $ => seq(
      choice(token.immediate('#{'), token.immediate('{')),
      $.expression,
      '}',
    ),

    _property_name_fragment: $ => token.immediate(/-?[_a-zA-Z][\w-]*/),

    interpolation: $ => seq(
      choice('#{', '{'),
      $.expression,
      '}',
    ),

    selector_interpolation: $ => seq(
      choice('#{', token.immediate('{')),
      $.expression,
      '}',
    ),

    object_value: $ => prec.dynamic(20, seq(
      '{',
      optional($._indent),
      optional($.object_entries),
      optional($._dedent),
      '}',
    )),

    object_entries: $ => prec.right(seq(
      $.object_entry,
      repeat(seq(repeat1($.object_entry_separator), $.object_entry)),
      repeat($.object_entry_separator),
    )),

    object_entry_separator: $ => choice(
      ',',
      $._newline,
    ),

    object_entry: $ => prec(PREC.OBJECT, seq(
      field('key', choice(
        $.identifier,
        $.dollar_identifier,
        $.string_value,
        $.number_value,
      )),
      ':',
      field('value', $.expression),
    )),

    unary_expression: $ => prec.right(PREC.UNARY, seq(
      choice(
        '!',
        '-',
        '+',
        '~',
      ),
      $.expression,
    )),

    binary_expression: $ => choice(
      prec.right(PREC.POWER, seq(
        field('left', $.expression),
        field('operator', '**'),
        field('right', $.expression),
      )),
      prec.dynamic(10, prec.left(PREC.EQUALITY, seq(
        field('left', $.expression),
        field('operator', $.is_not_operator),
        field('right', $.expression),
      ))),
      prec.dynamic(10, prec.left(PREC.TYPE_CHECK, seq(
        field('left', $.expression),
        field('operator', $.is_a_operator),
        field('right', $.expression),
      ))),
      prec.left(PREC.RANGE, seq(
        field('left', $.expression),
        field('operator', choice('..', '...')),
        field('right', $.expression),
      )),
      ...[
        ['||', PREC.LOGICAL],
        ['&&', PREC.LOGICAL],
        ['or', PREC.LOGICAL],
        ['and', PREC.LOGICAL],
        ['==', PREC.EQUALITY],
        ['!=', PREC.EQUALITY],
        ['isnt', PREC.EQUALITY],
        ['is', PREC.EQUALITY],
        ['in', PREC.IN],
        ['<', PREC.COMPARE],
        ['<=', PREC.COMPARE],
        ['>', PREC.COMPARE],
        ['>=', PREC.COMPARE],
        ['+', PREC.ADD],
        ['-', PREC.ADD],
        ['*', PREC.MULTIPLY],
        ['/', PREC.MULTIPLY],
        ['%', PREC.MULTIPLY],
      ].map(([operator, precedence]) =>
        prec.left(precedence, seq(
          field('left', $.expression),
          field('operator', operator),
          field('right', $.expression),
        )),
      ),
    ),

    _value: $ => choice(
      $.number_value,
      $.boolean_value,
      $.null_value,
      $.string_value,
      $.color_value,
      $.member_expression,
      $.variable_name,
      $.property_lookup,
      $.important_modifier,
      $.parenthesized_expression,
    ),

    parenthesized_expression: $ => seq('(', repeat1($.expression), ')'),

    member_expression: $ => prec(PREC.CALL, seq(
      field('object', $.variable_name),
      repeat1(choice(
        seq('.', field('property', $.identifier)),
        seq('[', field('property', choice(
          $.identifier,
          $.string_value,
          $.number_value,
        )), ']'),
      )),
    )),

    number_value: $ => prec.right(choice(
      seq(
        choice($.float_value, $.integer_value),
        alias(token.immediate('%'), $.unit),
      ),
      seq(
        choice($.float_value, $.integer_value),
        optional(choice(
          $.unit,
          alias($.known_unit, $.unit),
        )),
      ),
    )),

    integer_value: $ => /\d+/,
    float_value: $ => choice(/\d+\.\d+/, /\.\d+/),
    unit: $ => token.immediate(/[a-zA-Z]+/),
    known_unit: $ => token(prec(1, choice(
      'px',
      'em',
      'rem',
      'vh',
      'vw',
      'vmin',
      'vmax',
      'ch',
      'ex',
      'cm',
      'mm',
      'in',
      'pt',
      'pc',
      'fr',
      'deg',
      'rad',
      'turn',
      's',
      'ms',
    ))),
    variable_name: $ => choice(
      $.identifier,
      $.dollar_identifier,
    ),
    property_lookup: $ => /@-?[_a-zA-Z][\w-]*/,
    is_a_operator: $ => token(seq('is', /[ \t]+/, 'a')),
    is_not_operator: $ => token(seq('is', /[ \t]+/, 'not')),
    dollar_identifier: $ => /\$-?[_a-zA-Z][\w-]*/,
    custom_property_name: $ => /--[_a-zA-Z][\w-]*/,
    nested_property_name: $ => choice(
      'border',
      'font',
      'list',
      'text',
      'outline',
      'overflow',
      'animation',
      'transition',
      'flex',
      'grid',
      'place',
    ),
    boolean_value: $ => choice('true', 'false', 'yes', 'no'),
    null_value: $ => choice('null', 'nil'),
    important_modifier: $ => token(seq('!', 'important')),

    string_value: $ => choice(
      seq('"', repeat(choice(
        $.escape_sequence,
        token.immediate(/[^"\\\n]+/),
      )), '"'),
      seq('\'', repeat(choice(
        $.escape_sequence,
        token.immediate(/[^'\\\n]+/),
      )), '\''),
    ),

    escape_sequence: $ => token(seq('\\', /./)),

    raw_css_value: $ => token(prec(1, /[^:(),;{}"\'\/\s][^(),;{}"\'\/\n]*/)),

    keyframes_block: $ => seq(
      '{',
      optional($._indent),
      repeat(choice(
        $._newline,
        $.keyframe_block,
      )),
      optional($._dedent),
      '}',
    ),

    keyframe_block: $ => seq(
      field('selector', $.keyframe_selector),
      repeat(seq(',', field('selector', $.keyframe_selector))),
      choice($.block, $.css_declaration_block),
    ),

    keyframe_selector: $ => choice(alias($.identifier, $.tag_name), $.number_value),

    css_declaration_block: $ => prec.dynamic(20, seq(
      '{',
      optional($._indent),
      repeat(choice(
        $._newline,
        alias($.nested_rule_set, $.rule_set),
        alias($.css_declaration, $.declaration),
      )),
      optional($._dedent),
      '}',
    )),

    css_declaration: $ => prec.dynamic(20, prec.right(seq(
      field('property', declarationProperty($)),
      optional(choice(':', alias($._declaration_colon, ':'))),
      $._css_declaration_values,
      optional(';'),
      optional($._newline),
    ))),

    _css_declaration_values: $ => declarationValues($, $._css_declaration_value),

    _css_declaration_value: $ => choice(
      prec(1, $.call_expression),
      $.css_function_expression,
      $.raw_css_function,
      $.binary_expression,
      $.unary_expression,
      $.object_value,
      $.interpolation,
      $._value,
      $.raw_css_value,
    ),

    identifier: $ => /-?[_a-zA-Z][\w-]*/,

    comment: $ => token(choice(
      seq('//', /.*/),
      seq(
        '/*',
        /[^*]*\*+([^/*][^*]*\*+)*/,
        '/',
      ),
    )),
  },
});

function sep1(rule, separator) {
  return seq(rule, repeat(seq(separator, rule)));
}

function queriedBlock($, keyword) {
  return seq(
    keyword,
    field('query', choice($.expression, $.media_query)),
    $.block,
  );
}

function functionDefinition($) {
  return prec.dynamic(30, prec(PREC.CALL + 1, seq(
    field('name', alias($.identifier, $.function_name)),
    field('parameters', $.parameters),
    $.block,
  )));
}

function ruleSet($, selector) {
  return prec.dynamic(10, prec(3, seq(
    field('selector', selector),
    choice($.css_declaration_block, $.block),
  )));
}

function selectorSequence($) {
  return prec.right(repeat1(choice(
    alias($.identifier, $.tag_name),
    $.class_name,
    $.id_name,
    alias($.dollar_identifier, $.placeholder_selector),
    $.pseudo_element,
    $.pseudo_class,
    $.attribute_selector,
    $.nesting_selector_with_suffix,
    $.deep_combinator,
    $.combinator_selector,
    $.nesting_selector,
    $.universal_selector,
    $.selector_interpolation,
  )));
}

function declarationRule($, property, optionalColon = true) {
  const parts = [field('property', property)];
  if (optionalColon) {
    parts.push(optional(choice(':', alias($._declaration_colon, ':'))));
  }
  parts.push($._declaration_values);
  return seq(...parts);
}

function declarationProperty($) {
  return choice(
    $.interpolated_property_name,
    alias($.identifier, $.property_name),
    alias($.custom_property_name, $.property_name),
    alias($.nested_property_name, $.property_name),
  );
}

function declarationValues($, value) {
  return prec.right(seq(
    field('value', value),
    repeat(choice(
      field('value', value),
      seq(',', repeat($._newline), field('value', value)),
    )),
  ));
}
