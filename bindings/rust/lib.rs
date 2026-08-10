use tree_sitter_language::LanguageFn;

extern "C" {
    fn tree_sitter_stylus() -> *const ();
}

pub fn language() -> LanguageFn {
    unsafe { LanguageFn::from_raw(tree_sitter_stylus) }
}
