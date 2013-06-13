//
// Sample asynchronous C++ Node.js extention.
// This is initially modeled after https://github.com/kkaefer/node-cpp-modules
// Thank you to kkaefer.
//

#include "seed-node-c-module.hpp"

using namespace v8;


// We use a struct to store information about the asynchronous "work request".
struct Baton {
  // This handle holds the callback function we'll call after the work request
  // has been completed in a threadpool thread. It's persistent so that V8
  // doesn't garbage collect it away while our request waits to be processed.
  // This means that we'll have to dispose of it later ourselves.
  Persistent<Function> callback;

  // Tracking errors that happened in the worker function. You can use any
  // variables you want. E.g. in some cases, it might be useful to report
  // an error number.
  bool error;
  std::string error_message;

  // Hold the number of milliseconds JavaScriptLand wants to sleep
  int32_t sleepTime;

  // Custom data to return
  int32_t result;
};

// This is the function called directly from JavaScript land. It creates a
// work request object and schedules it for execution.
Handle<Value> Async(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::Error(String::New("Requires two parameters: number, callback")));
  }

  // BUGBUG - need to check for valid number
  // if (!args[0]...

  if (!args[1]->IsFunction()) {
    return ThrowException(Exception::TypeError(String::New("First argument must be a callback function")));
  }

  Local<Integer> integer = args[0]->ToInteger();
  int32_t sleepTime = integer->Value();

  // There's no ToFunction(), use a Cast instead.
  Local<Function> callback = Local<Function>::Cast(args[1]);

  // The baton holds our custom status information for this asynchronous call,
  // like the callback function we want to call when returning to the main
  // thread and the status information.
  Baton* baton = new Baton();
  baton->error = false;
  baton->callback = Persistent<Function>::New(callback);
  baton->sleepTime = sleepTime;

  // This creates the work request struct.
  uv_work_t *req = new uv_work_t();
  req->data = baton;

  // Schedule our work request with libuv. Here you can specify the functions
  // that should be executed in the threadpool and back in the main thread
  // after the threadpool function completed.
  int status = uv_queue_work(uv_default_loop(), req, AsyncWork,
			     (uv_after_work_cb)AsyncAfter);
  assert(status == 0);

  return Undefined();
}

// This function is executed in another thread at some point after it has been
// scheduled. IT MUST NOT USE ANY V8 FUNCTIONALITY. Otherwise your extension
// will crash randomly and you'll have a lot of fun debugging.
// If you want to use parameters passed into the original call, you have to
// convert them to PODs or some other fancy method.
void AsyncWork(uv_work_t* req) {
  Baton* baton = static_cast<Baton*>(req->data);

  // Do work in threadpool here.
  // Sleep for the specified milliseconds. Note that usleep takes microseconds.
  usleep(baton->sleepTime * 1000);

  // Return the sleep time
  baton->result = baton->sleepTime;

  // If the work we do fails, set baton->error_message to the error string
  // and baton->error to true.
}

// This function is executed in the main V8/JavaScript thread. That means it's
// safe to use V8 functions again. Don't forget the HandleScope!
void AsyncAfter(uv_work_t* req) {
  HandleScope scope;
  Baton* baton = static_cast<Baton*>(req->data);

  if (baton->error) {
    Local<Value> err = Exception::Error(String::New(baton->error_message.c_str()));

    // Prepare the parameters for the callback function.
    const unsigned argc = 1;
    Local<Value> argv[argc] = { err };

    // Wrap the callback function call in a TryCatch so that we can call
    // node's FatalException afterwards. This makes it possible to catch
    // the exception from JavaScript land using the
    // process.on('uncaughtException') event.
    TryCatch try_catch;
    baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
    if (try_catch.HasCaught()) {
      node::FatalException(try_catch);
    }
  } else {
    // In case the operation succeeded, convention is to pass null as the
    // first argument before the result arguments.
    // In case you produced more complex data, this is the place to convert
    // your plain C++ data structures into JavaScript/V8 data structures.
    const unsigned argc = 2;
    Local<Value> argv[argc] = {
      Local<Value>::New(Null()),
      Local<Value>::New(Integer::New(baton->result))
    };

    // Wrap the callback function call in a TryCatch so that we can call
    // node's FatalException afterwards. This makes it possible to catch
    // the exception from JavaScript land using the
    // process.on('uncaughtException') event.
    TryCatch try_catch;
    baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
    if (try_catch.HasCaught()) {
      node::FatalException(try_catch);
    }
  }

  // The callback is a permanent handle, so we have to dispose of it manually.
  baton->callback.Dispose();

  // We also created the baton and the work_req struct with new, so we have to
  // manually delete both.
  delete baton;
  delete req;
}

void RegisterModule(Handle<Object> target) {
  target->Set(String::NewSymbol("async"),
	      FunctionTemplate::New(Async)->GetFunction());
}

NODE_MODULE(seedmodule, RegisterModule);
