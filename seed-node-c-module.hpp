#ifndef SEEDMODULE_HPP
#define SEEDMODULE_HPP

#include <node.h>
#include <string>

v8::Handle<v8::Value> Async(const v8::Arguments& args);
void AsyncWork(uv_work_t* req);
void AsyncAfter(uv_work_t* req);


#endif
