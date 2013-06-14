#ifndef SEEDMODULE_HPP
#define SEEDMODULE_HPP

#include <node.h>
#include <string>

v8::Handle<v8::Value> Sleep(const v8::Arguments& args);
void AsyncWork(uv_work_t * request);
void AsyncAfter(uv_work_t * request);

#endif
