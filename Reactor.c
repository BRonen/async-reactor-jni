#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "Reactor.h"

void assert(int pred, char* err) {
    if (pred)
        return;

    printf("[ASSERT] %s\n", err);
    exit(EXIT_FAILURE);
}

typedef void (*ev_callback)(int fd, uint32_t events);

typedef struct watcher {
  int fd;
  uint32_t mask;
  jobject callback;
  void *userdata;
} watcher_t;

typedef struct {
  int epfd;
  struct epoll_event *events;
  int max_events;
} reactor_t;

JNIEXPORT jlong JNICALL Java_Reactor_create_1reactor(JNIEnv* _env, jobject _this, jint max_events) {
  printf("[C] Java_Reactor_create_1reactor\n");
  reactor_t* r = malloc(sizeof(reactor_t));
  assert(r != NULL, "Error while trying to alloc reactor");

  r->epfd = epoll_create1(0);
  assert(r->epfd != -1, "Error while trying to open epoll");

  r->events = calloc(max_events, sizeof(struct epoll_event));
  assert(r->events != NULL, "Error while trying to alloc reactor->events");

  r->max_events = max_events;

  return (long) r;
}

JNIEXPORT void JNICALL Java_Reactor_free_1reactor(JNIEnv* _env, jobject _this, jlong reactor_ptr) {
  printf("[C] Java_Reactor_free_1reactor\n");
  reactor_t* r = (reactor_t*) reactor_ptr;

  int e = close(r->epfd);
  assert(e != -1, "Error while trying to close epoll");

  free(r->events);
  free(r);
}

JNIEXPORT void JNICALL Java_Reactor_reactor_1run(JNIEnv* env, jobject _this, jlong reactor_ptr, jint timeout) {
  printf("[C] Java_Reactor_reactor_1run\n");
  reactor_t* r = (reactor_t*) reactor_ptr;

  int n = epoll_wait(r->epfd, r->events, r->max_events, timeout);

  for (int i = 0; i < n; i++) {
    struct epoll_event *ev = &r->events[i];
    watcher_t *w = ev->data.ptr;

    jclass callback_class = (*env)->GetObjectClass(env, w->callback);
    assert(callback_class != NULL, "Error while trying to get callback class");

    jmethodID callback_id = (*env)->GetMethodID(env, callback_class, "accept", "(Ljava/lang/Object;Ljava/lang/Object;)V");
    assert(callback_id != NULL, "Error while trying to get callback method id");

    jclass integer_class = (*env)->FindClass(env, "java/lang/Integer");
    assert(integer_class != NULL, "Error while trying to get the integer class");

    jmethodID integer_init = (*env)->GetMethodID(env, integer_class, "<init>", "(I)V");
    assert(callback_class != NULL, "Error while trying to get integer init method");

    jobject jfd = (*env)->NewObject(env, integer_class, integer_init, w->fd);
    jobject jevents = (*env)->NewObject(env, integer_class, integer_init, ev->events);

    (*env)->CallVoidMethod(env, w->callback, callback_id, jfd, jevents);

    (*env)->DeleteLocalRef(env, callback_class);
  }
}

JNIEXPORT jlong JNICALL Java_Reactor_create_1watcher(
  JNIEnv* env, jobject _this, jlong reactor_ptr, jint fd, jint events, jobject watcher_callback
) {
  printf("[C] Java_Reactor_create_1watcher\n");
  reactor_t* r = (reactor_t*) reactor_ptr;

  watcher_t* w = malloc(sizeof(watcher_t));
  assert(w != NULL, "Error while trying to alloc watcher");

  w->fd = fd;
  w->mask = events;
  w->callback = (*env)->NewGlobalRef(env, watcher_callback);

  struct epoll_event ev = {0};
  ev.events = events;
  ev.data.ptr = w;

  epoll_ctl(r->epfd, EPOLL_CTL_ADD, fd, &ev);

  return (jlong) w;
}


JNIEXPORT void JNICALL Java_Reactor_free_1watcher(JNIEnv* env, jobject _this, jlong watcher) {
  printf("[C] Java_Reactor_free_1watcher\n");
  watcher_t* w = (watcher_t*) watcher;

  int e = close(w->fd);
  assert(e != -1, "Error while trying to free watcher file descriptor");

  (*env)->DeleteGlobalRef(env, w->callback);

  free(w);
}

JNIEXPORT jint JNICALL Java_Reactor_create_1eventfd(JNIEnv* _env, jobject _this) {
  printf("[C] Java_Reactor_create_1eventfd\n");
  int efd = eventfd(0, EFD_NONBLOCK);
  assert(efd != -1, "Error while trying to open event file descriptor");

  return efd;
}

JNIEXPORT void JNICALL Java_Reactor_free_1eventfd(JNIEnv* _env, jobject _this, jint efd) {
  printf("[C] Java_Reactor_free_1eventfd\n");
  int e = close(efd);
  assert(e != -1, "Error while trying to close eventfd");
}

JNIEXPORT void JNICALL Java_Reactor_inc_1eventfd(JNIEnv* _env, jobject _this, jint efd) {
  printf("[C] Java_Reactor_inc_1eventfd - %d\n", efd);
  uint64_t value = 1;

  int e = write(efd, &value, sizeof(value));
  assert(e != -1, "Error while trying to inc eventfd");
}

JNIEXPORT jlong JNICALL Java_Reactor_read_1eventfd(JNIEnv* _env, jobject _this, jint efd) {
  printf("[C] Java_Reactor_read_1eventfd - %d\n", efd);
  uint64_t value;

  int e = read(efd, &value, sizeof(value));
  assert(e != -1, "Error while trying to read eventfd");

  return value;
}
