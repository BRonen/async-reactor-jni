#include <fcntl.h>
#include <jni.h>
#include <liburing.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "Reactor.h"

#define READ_FILE_OP 1
#define WRITE_FILE_OP 2
#define EVENT_OP 3

void assert(int pred, char* err) {
    if (pred)
        return;

    printf("[ASSERT] %s\n", err);
    exit(EXIT_FAILURE);
}

typedef void (*ev_callback)(int fd, uint32_t events);

typedef struct {
  jobject on_complete;
  void* user_data;
  unsigned int operation_type;
} task_t;

typedef struct {
  struct io_uring queue;
  int ring_size;
} reactor_t;

JNIEXPORT jlong JNICALL Java_Reactor_create_1reactor(JNIEnv* _env, jobject _this, jint ring_size) {
  printf("[C] Java_Reactor_create_1reactor\n");
  reactor_t* r = malloc(sizeof(reactor_t));
  assert(r != NULL, "Error while trying to alloc reactor");

  int e = io_uring_queue_init(ring_size, &r->queue, 0);
  assert(e != -1, "Error while trying to init io_uring queue");

  r->ring_size = ring_size;

  return (long) r;
}

JNIEXPORT void JNICALL Java_Reactor_free_1reactor(JNIEnv* _env, jobject _this, jlong reactor_ptr) {
  printf("[C] Java_Reactor_free_1reactor\n");
  reactor_t* r = (reactor_t*) reactor_ptr;

  io_uring_queue_exit(&r->queue);

  free(r);
}

JNIEXPORT void JNICALL Java_Reactor_reactor_1run(JNIEnv* env, jobject _this, jlong reactor_ptr, jint timeout) {
  printf("[C] Java_Reactor_reactor_1run\n");
  reactor_t* r = (reactor_t*) reactor_ptr;
  struct io_uring_cqe* cqe;

  if (timeout >= 0) {
    struct __kernel_timespec ts = {};
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000000;

    int e = io_uring_wait_cqe_timeout(&r->queue, &cqe, &ts);

    if(e == -ETIME)return;
    assert(e >= 0, "Error while trying to io_uring_wait_cqe_timeout");
  } else {
    int e = io_uring_wait_cqe(&r->queue, &cqe);

    if(e == -ETIME) return;
    assert(e >= 0, "Error while trying to io_uring_wait_cqe");
  }

  task_t* task = (task_t*)cqe->user_data;
  assert(cqe->res >= 0, "Error returned by I/O operation");

  printf("[C] Task received: %d\n", task->operation_type);
  if (task->operation_type == READ_FILE_OP || task->operation_type == WRITE_FILE_OP) {
    int buffer_length = cqe->res;

    jclass integer_class = (*env)->FindClass(env, "java/lang/Integer");
    assert(integer_class != NULL, "Error while trying to get the integer class");

    jmethodID integer_init = (*env)->GetMethodID(env, integer_class, "<init>", "(I)V");
    assert(integer_init != NULL, "Error while trying to get integer init method");

    jobject jbuffer_length = (*env)->NewObject(env, integer_class, integer_init, buffer_length);
    assert(jbuffer_length != NULL, "Error while trying to get integer instance to buffer length");

    jobject jbuffer = (jobject) task->user_data;

    jclass callback_class = (*env)->GetObjectClass(env, task->on_complete);
    assert(callback_class != NULL, "Error while trying to get callback class");

    jmethodID callback_id = (*env)->GetMethodID(env, callback_class, "accept", "(Ljava/lang/Object;Ljava/lang/Object;)V");
    assert(callback_id != NULL, "Error while trying to get callback method id");

    (*env)->CallVoidMethod(env, task->on_complete, callback_id, jbuffer, jbuffer_length);

    (*env)->DeleteGlobalRef(env, jbuffer);
    (*env)->DeleteGlobalRef(env, task->on_complete);
    (*env)->DeleteLocalRef(env, integer_class);
    (*env)->DeleteLocalRef(env, jbuffer_length);
    (*env)->DeleteLocalRef(env, callback_class);

    free(task);
  } else if (task->operation_type == EVENT_OP) {
    int efd = (int) (long) task->user_data;

    uint64_t value = 0;
    int e = read(efd, &value, sizeof(value));
    assert(e != -1, "Error while trying to read eventfd");

    jclass callback_class = (*env)->GetObjectClass(env, task->on_complete);
    assert(callback_class != NULL, "Error while trying to get callback class");

    jmethodID callback_id = (*env)->GetMethodID(env, callback_class, "accept", "(J)V");
    assert(callback_id != NULL, "Error while trying to get callback method id");

    (*env)->CallVoidMethod(env, task->on_complete, callback_id, value);

    (*env)->DeleteGlobalRef(env, task->on_complete);
    (*env)->DeleteLocalRef(env, callback_class);

    int ec = close(efd);
    assert(ec != -1, "Error while trying to close eventfd");

    free(task);
  }

  io_uring_cqe_seen(&r->queue, cqe);
}

JNIEXPORT jint JNICALL Java_Reactor_open(JNIEnv* env, jobject _this, jstring jpath, jint flags) {
  printf("[C] Java_Reactor_open\n");
  const char* path = (*env)->GetStringUTFChars(env, jpath, NULL);
    assert(path != NULL, "Error trying to get UTFChars from file path");

    int fd = open(path, flags);

    (*env)->ReleaseStringUTFChars(env, jpath, path);
    assert(fd >= 0, "Error while trying to call open");

    return (jint)fd;
}

JNIEXPORT void JNICALL Java_Reactor_close(JNIEnv* env, jobject _this, jint fd) {
  printf("[C] Java_Reactor_close\n");
  int e = close(fd);
  assert(e != -1, "Error trying to close fd");
}

JNIEXPORT jlong JNICALL Java_Reactor_file_1read(
  JNIEnv* env, jobject _this, jlong reactor_ptr, jint fd, jint length, jint offset, jobject on_complete
) {
  printf("[C] Java_Reactor_file_1read\n");
  reactor_t* r = (reactor_t*) reactor_ptr;

  jclass bytebuffer_class = (*env)->FindClass(env, "java/nio/ByteBuffer");
  assert(bytebuffer_class != NULL, "Error while trying to find ByteBuffer class");

  jmethodID alloc_direct_id = (*env)->GetStaticMethodID(env, bytebuffer_class, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
  assert(alloc_direct_id != NULL, "Error while trying to find alloc_direct_id method id");

  jobject jbuffer = (*env)->CallStaticObjectMethod(env, bytebuffer_class, alloc_direct_id, length);

  void* buffer = (*env)->GetDirectBufferAddress(env, jbuffer);

  task_t* task = malloc(sizeof(task_t));
  assert(task != NULL, "Error while trying to alloc watcher");

  struct io_uring_sqe* sqe = io_uring_get_sqe(&r->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  task->on_complete = (*env)->NewGlobalRef(env, on_complete);
  task->user_data = (*env)->NewGlobalRef(env, jbuffer);
  task->operation_type = READ_FILE_OP;
  io_uring_prep_read(sqe, fd, buffer, length, offset);

  sqe->user_data = (uint64_t) task;
  int e = io_uring_submit(&r->queue);
  assert(e != -1, "Error while trying to io_uring_submit");

  (*env)->DeleteLocalRef(env, bytebuffer_class);
  (*env)->DeleteLocalRef(env, jbuffer);
  (*env)->DeleteLocalRef(env, on_complete);

  return (jlong) task;
}

JNIEXPORT jlong JNICALL Java_Reactor_file_1write(
  JNIEnv* env, jobject _this, jlong reactor_ptr, jint fd, jobject jbuffer, jint length, jint offset, jobject on_complete
) {
    printf("[C] Java_Reactor_file_1write\n");
    reactor_t* r = (reactor_t*) reactor_ptr;

    void* buffer_ptr = (*env)->GetDirectBufferAddress(env, jbuffer);
    assert(buffer_ptr != NULL, "Error trying to get Direct Buffer address");

    task_t* task = malloc(sizeof(task_t));
    assert(task != NULL, "Error while trying to alloc task");

    task->on_complete = (*env)->NewGlobalRef(env, on_complete);
    task->user_data = (*env)->NewGlobalRef(env, jbuffer);
    task->operation_type = WRITE_FILE_OP;

    struct io_uring_sqe* sqe = io_uring_get_sqe(&r->queue);
    assert(sqe != NULL, "Error while trying to get submission queue entry");

    io_uring_prep_write(sqe, fd, buffer_ptr, length, offset);

    sqe->user_data = (uint64_t) task;

    int e = io_uring_submit(&r->queue);
    assert(e != -1, "Error while trying to io_uring_submit");

    (*env)->DeleteLocalRef(env, on_complete);
    (*env)->DeleteLocalRef(env, jbuffer);

    return (jlong) task;
}

JNIEXPORT jint JNICALL Java_Reactor_create_1eventfd(JNIEnv* _env, jobject _this) {
  printf("[C] Java_Reactor_create_1eventfd\n");
  int efd = eventfd(0, EFD_NONBLOCK);
  assert(efd != -1, "Error while trying to open event file descriptor");

  return efd;
}

JNIEXPORT void JNICALL Java_Reactor_trigger_1eventfd(JNIEnv* _env, jobject _this, jint efd, jlong value) {
  printf("[C] Java_Reactor_trigger_1eventfd - %d\n", efd);

  int e = write(efd, &value, sizeof(value));
  assert(e != -1, "Error while trying to inc eventfd");
}

JNIEXPORT void JNICALL Java_Reactor_listen_1eventfd(JNIEnv* env, jobject _this, jlong reactor_ptr, jint efd, jobject on_complete) {
  printf("[C] Java_Reactor_listen_1eventfd - %d\n", efd);

  reactor_t* r = (reactor_t*) reactor_ptr;

  task_t* task = malloc(sizeof(task_t));
  assert(task != NULL, "Error while trying to alloc watcher");

  struct io_uring_sqe* sqe = io_uring_get_sqe(&r->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  task->on_complete = (*env)->NewGlobalRef(env, on_complete);
  task->user_data = (void*) (long) efd;
  task->operation_type = EVENT_OP;

  io_uring_prep_poll_add(sqe, efd, POLLIN);

  sqe->user_data = (uint64_t) task;

  int e = io_uring_submit(&r->queue);
  assert(e != -1, "Error while trying to io_uring_submit");
}
