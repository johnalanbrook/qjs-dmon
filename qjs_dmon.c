#include "quickjs.h"

#define DMON_IMPL
#include "dmon.h"

// Define the file event structure and completion queue
typedef struct {
  dmon_action action;
  char rootdir[256];
  char filepath[256];
  char oldfilepath[256];
} FileEvent;

typedef struct EventNode {
  FileEvent event;
  struct EventNode *next;
} EventNode;

typedef struct {
  EventNode *head;
  EventNode *tail;
} CompletionQueue;

CompletionQueue completionQueue = { NULL, NULL };

// Helper functions for the completion queue
void enqueue_event(FileEvent event) {
  EventNode *node = malloc(sizeof(EventNode));
  node->event = event;
  node->next = NULL;

  if (completionQueue.tail) {
    completionQueue.tail->next = node;
  } else {
    completionQueue.head = node;
  }
  completionQueue.tail = node;
}

int dequeue_event(FileEvent *event) {
  if (!completionQueue.head) {
    return 0; // No event
  }
  EventNode *node = completionQueue.head;
  *event = node->event;
  completionQueue.head = node->next;
  if (!completionQueue.head) {
    completionQueue.tail = NULL;
  }
  free(node);
  return 1;
}

void watch_cb(dmon_watch_id id, dmon_action action, const char *rootdir, const char *filepath, const char *oldfilepath, void *user)
{
  FileEvent event;
  event.action = action;
  strncpy(event.rootdir, rootdir, sizeof(event.rootdir) - 1);
  strncpy(event.filepath, filepath, sizeof(event.filepath) - 1);
  if (oldfilepath) {
    strncpy(event.oldfilepath, oldfilepath, sizeof(event.oldfilepath) - 1);
  } else {
    event.oldfilepath[0] = '\0';
  }
  enqueue_event(event); // Add event to completion queue
}

static dmon_watch_id watched = {0};

JSValue js_dmon_watch(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  if (watched.id)
    return JS_ThrowReferenceError(js, "Already watching a directory.");
    
  const char *dir = JS_ToCString(js,argv[0]);
  watched = dmon_watch(dir,watch_cb, DMON_WATCHFLAGS_RECURSIVE, NULL);
  printf("watch id is %lu\n", watched.id);
  return JS_UNDEFINED;
}

JSValue js_dmon_unwatch(JSContext *js, JSValue self, int argc, JSValue *argv)
{
  if (!watched.id)
    return JS_ThrowReferenceError(js, "Not watching a directory.");
    
  dmon_unwatch(watched);
  watched.id = 0;
  return JS_UNDEFINED;
}

JSValue js_dmon_poll(JSContext *js, JSValueConst this_val, int argc, JSValueConst *argv) {
  FileEvent event;
  while (dequeue_event(&event)) {
    JSValue jsevent = JS_NewObject(js);
    JSValue action;
    switch(event.action) {
      case DMON_ACTION_CREATE:
        action = JS_NewAtomString(js, "create");
        break;
      case DMON_ACTION_DELETE:
        action = JS_NewAtomString(js, "delete");
        break;
      case DMON_ACTION_MODIFY:
        action = JS_NewAtomString(js, "modify");
        break;
      case DMON_ACTION_MOVE:
        action = JS_NewAtomString(js, "move");
        break;
    }
    JS_SetPropertyStr(js, jsevent, "action", action);
    JS_SetPropertyStr(js, jsevent, "root", JS_NewString(js, event.rootdir));
    JS_SetPropertyStr(js, jsevent, "file", JS_NewString(js, event.filepath));
    JS_SetPropertyStr(js, jsevent, "old", JS_NewString(js, event.oldfilepath));
    JS_Call(js, argv[0], JS_UNDEFINED, 1, &jsevent);
  }
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_dmon_funcs[] = {
  JS_CFUNC_DEF("watch", 1, js_dmon_watch),
  JS_CFUNC_DEF("unwatch", 0, js_dmon_unwatch),
  JS_CFUNC_DEF("poll", 1, js_dmon_poll)
};

JSValue js_dmon(JSContext *js)
{
  JSValue export = JS_NewObject(js);
  JS_SetPropertyFunctionList(js, export, js_dmon_funcs, sizeof(js_dmon_funcs)/sizeof(JSCFunctionListEntry));

  dmon_init();
  
  return export;
}

static int js_dmon_init(JSContext *js, JSModuleDef *m) {
  JS_SetModuleExport(js, m, "default",js_dmon(js));
  return 0;
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_dmon
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *js, const char *module_name) {
  JSModuleDef *m = JS_NewCModule(js, module_name, js_dmon_init);
  if (!m) return NULL;
  JS_AddModuleExport(js, m, "default");
  return m;
}
