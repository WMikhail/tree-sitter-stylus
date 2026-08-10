fn main() {
    let mut build = cc::Build::new();
    build.include("src").file("src/parser.c").file("src/scanner.c");
    build.flag_if_supported("-std=c11");
    build.compile("tree-sitter-stylus");

    println!("cargo:rerun-if-changed=src/parser.c");
    println!("cargo:rerun-if-changed=src/scanner.c");
}
