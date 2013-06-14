//
// Sample asynchronous C++ Node.js extention.
// This is initially modeled after https://github.com/kkaefer/node-cpp-modules
// Thank you to kkaefer.
//

#include "seed-node-c-module.hpp"


//
// Structure used to store asynchronous call data
//
struct SleepData {
  // This handle holds the callback function we'll call after the work request
  // has been completed in a threadpool thread. It's persistent so that V8
  // doesn't garbage collect it away while our request waits to be processed.
  // This means that we'll have to dispose of it later ourselves.
  v8::Persistent<v8::Function> callbackFn;

  // Error handling
  bool isError;
  std::string errorMessage;

  // Hold the number of milliseconds JavaScriptLand wants to sleep
  int32_t sleepTime;

  // Result data to return to JavaScriptLand - number of milliseconds slept
  int32_t sleptTime;
};


//
// Sleep is called directly from JavaScriptLand. Creates a work
// request object and schedules it for execution off the main thread.
//
v8::Handle<v8::Value> Sleep(const v8::Arguments& args) {
  v8::HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(v8::Exception::Error(v8::String::New("Sleep requires two parameters: number, callback")));
  }
  if (!args[0]->IsInt32()) {
    return ThrowException(v8::Exception::TypeError(v8::String::New("Second argument must be an integer")));
  }
  if (!args[1]->IsFunction()) {
    return ThrowException(v8::Exception::TypeError(v8::String::New("First argument must be a callback function")));
  }

  v8::Local<v8::Integer> integer = args[0]->ToInteger();
  int32_t sleepTime = integer->Value();

  // There's no ToFunction(), so use a Cast instead.
  v8::Local<v8::Function> callbackFn = v8::Local<v8::Function>::Cast(args[1]);

  //
  // Create a SleepData structure to hold status information for the asynchronous call
  //
  SleepData * sleepData = new SleepData();
  sleepData->isError = false;
  sleepData->callbackFn = v8::Persistent<v8::Function>::New(callbackFn);
  sleepData->sleepTime = sleepTime;

  // This creates the work request structure and will need to be deleted in AsyncAfter
  uv_work_t * request = new uv_work_t();
  request->data = sleepData;

  // Schedule our work request with libuv. Here you can specify the functions
  // that should be executed in the threadpool and back in the main thread
  // after the threadpool function completed.
  int status = uv_queue_work(uv_default_loop(), request, AsyncWork, (uv_after_work_cb)AsyncAfter);
  assert(status == 0);

  return v8::Undefined();
}

//
// This function is executed in another thread at some point after it has been
// scheduled. IT MUST NOT USE ANY V8 FUNCTIONALITY. Otherwise your extension
// will crash randomly and you'll have a lot of fun debugging.
// If you want to use parameters passed into the original call, you have to
// convert them to PODs or some other fancy method.
//
void AsyncWork(uv_work_t * request) {
  SleepData * sleepData = static_cast<SleepData *>(request->data);

  //
  // Here is where we'd do CPU intensive work. For this sample, we merely
  // sleep for the time specified in the JavaScriptLand call into us. Note
  // that this will not block Node since we're now on a threadpool thread.
  //
  usleep(sleepData->sleepTime * 1000);

  // Return the sleep time
  sleepData->sleptTime = sleepData->sleepTime;

  // If the work we do fails, set sleepData->errorMessage to the error string
  // and sleepData->error to true.
}

//
// This function is executed in the main V8/JavaScript thread. That means it's
// safe to use V8 functions again. Don't forget the HandleScope!
//
void AsyncAfter(uv_work_t * request) {
  v8::HandleScope scope;
  SleepData * sleepData = static_cast<SleepData *>(request->data);

  if (sleepData->isError) {
    v8::Local<v8::Value> err = v8::Exception::Error(v8::String::New(sleepData->errorMessage.c_str()));

    // Prepare the parameters for the callback function.
    const unsigned argc = 1;
    v8::Local<v8::Value> argv[argc] = { err };

    // Wrap the callback function call in a TryCatch so that we can call
    // Node's FatalException afterwards. This makes it possible to catch
    // the exception from JavaScriptLand using the
    // process.on('uncaughtException') event.
    v8::TryCatch try_catch;
    sleepData->callbackFn->Call(v8::Context::GetCurrent()->Global(), argc, argv);
    if (try_catch.HasCaught()) {
      node::FatalException(try_catch);
    }
  } 
  else {
    //
    // In the case of the operation succeeded, the convention is to pass null as the
    // first argument before the result arguments.
    // In case we produced more complex data, this is the place to convert
    // your our C++ data structures into JavaScript/V8 data structures.
    //
    const unsigned argc = 2;
    v8::Local<v8::Value> argv[argc] = {
      v8::Local<v8::Value>::New(v8::Null()),
      v8::Local<v8::Value>::New(v8::Integer::New(sleepData->sleptTime))
    };

    //
    // Wrap the callback function call in a TryCatch so that we can call
    // Node's FatalException afterwards. This makes it possible to catch
    // the exception from JavaScript land using the
    // process.on('uncaughtException') event.
    //
    v8::TryCatch try_catch;
    sleepData->callbackFn->Call(v8::Context::GetCurrent()->Global(), argc, argv);
    if (try_catch.HasCaught()) {
      node::FatalException(try_catch);
    }
  }

  // The callback is a permanent handle, so we have to dispose of it manually.
  sleepData->callbackFn.Dispose();

  // We also created the sleepData and the work_req struct with new, so we have to
  // manually delete both.
  delete sleepData;
  delete request;
}

void RegisterModule(v8::Handle<v8::Object> target) {
  target->Set(v8::String::NewSymbol("sleep"), v8::FunctionTemplate::New(Sleep)->GetFunction());
}

NODE_MODULE(seedmodule, RegisterModule);
