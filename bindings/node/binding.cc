#include "tree_sitter/parser.h"
#include <nan.h>

extern "C" TSLanguage *tree_sitter_stylus();

namespace {

NAN_METHOD(GetLanguage) {
  info.GetReturnValue().Set(Nan::New<v8::External>(tree_sitter_stylus()));
}

NAN_MODULE_INIT(Init) {
  Nan::SetMethod(target, "language", GetLanguage);
}

NODE_MODULE(tree_sitter_stylus_binding, Init)

}  // namespace
