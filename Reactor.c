#include <fcntl.h>
#include <jni.h>
#include <liburing.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "Reactor.h"

#define READ_FILE_OP 1
#define WRITE_FILE_OP 2
#define EVENT_OP 3
#define TIMER_OP 4

#define fd_t jint
#define ptr_t jlong

void assert(int pred, char* err) {
  if (pred) return;

  printf("[ASSERT] %s\n", err);
  exit(EXIT_FAILURE);
}

typedef struct {
  int tfd;
  uint64_t *expirations;
  struct itimerspec *timerspec;
} timer_data_t;

typedef struct {
  jobject on_complete;
  void* user_data;
  unsigned int operation_type;
} task_t;

typedef struct {
  struct io_uring queue;
  int ring_size;
} reactor_t;

// Reactor

JNIEXPORT ptr_t JNICALL Java_Reactor_create_1reactor(JNIEnv* _env, jobject _this, jint ring_size) {
  printf("[C] Java_Reactor_create_1reactor\n");
  reactor_t* reactor = malloc(sizeof(reactor_t));
  assert(reactor != NULL, "Error while trying to alloc reactor");

  assert(io_uring_queue_init(ring_size, &reactor->queue, 0) != -1, "Error while trying to init io_uring queue");

  reactor->ring_size = ring_size;

  return (long) reactor;
}

JNIEXPORT void JNICALL Java_Reactor_free_1reactor(JNIEnv* _env, jobject _this, ptr_t reactor_ptr) {
  printf("[C] Java_Reactor_free_1reactor\n");
  reactor_t* reactor = (reactor_t*) reactor_ptr;

  io_uring_queue_exit(&reactor->queue);

  free(reactor);
}

// Event FD

JNIEXPORT fd_t JNICALL Java_Reactor_create_1eventfd(JNIEnv* _env, jobject _this, jint flags) {
  printf("[C] Java_Reactor_create_1eventfd\n");
  fd_t efd = eventfd(0, flags);
  assert(efd != -1, "Error while trying to open event file descriptor");

  return efd;
}

JNIEXPORT void JNICALL Java_Reactor_trigger_1eventfd(JNIEnv* _env, jobject _this, fd_t efd, jlong value) {
  printf("[C] Java_Reactor_trigger_1eventfd - %d\n", efd);

  int e = write(efd, &value, sizeof(value));
  assert(e != -1, "Error while trying to inc eventfd");
}

JNIEXPORT void JNICALL Java_Reactor_listen_1eventfd(
  JNIEnv* env, jobject _this, ptr_t reactor_ptr, fd_t efd, jobject on_complete
) {
  printf("[C] Java_Reactor_listen_1eventfd - %d\n", efd);

  reactor_t* reactor = (reactor_t*) reactor_ptr;

  task_t* task = malloc(sizeof(task_t));
  assert(task != NULL, "Error while trying to alloc watcher");

  struct io_uring_sqe* sqe = io_uring_get_sqe(&reactor->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  task->on_complete = (*env)->NewGlobalRef(env, on_complete);
  task->user_data = (void*) (long) efd;
  task->operation_type = EVENT_OP;

  io_uring_prep_poll_add(sqe, efd, POLLIN);

  sqe->user_data = (uint64_t) task;

  int e = io_uring_submit(&reactor->queue);
  assert(e != -1, "Error while trying to io_uring_submit");
}

// Timer FD

JNIEXPORT ptr_t JNICALL Java_Reactor_create_1timer(
  JNIEnv *env, jobject _this, ptr_t reactor_ptr, jint value, jint interval,
  jint clock_type, jint fd_flags, jint timer_flags, jobject on_complete
) {
  printf("[C] Java_Reactor_create_1timer\n");

  reactor_t *reactor = (reactor_t*) reactor_ptr;

  int tfd = timerfd_create(clock_type, fd_flags);
  assert(tfd != -1, "Error while trying to open timer file descriptor");

  struct itimerspec *ts = calloc(1, sizeof(struct itimerspec));
  assert(ts != NULL, "Error while trying to alloc itimerspec");

  ts->it_value.tv_sec = value;
  ts->it_interval.tv_sec = interval;

  assert(timerfd_settime(tfd, timer_flags, ts, NULL) != -1, "Error while trying to set time on timer file descriptor");

  uint64_t *expirations = malloc(sizeof(uint64_t));

  timer_data_t *timer_data = malloc(sizeof(timer_data_t));
  timer_data->tfd = tfd;
  timer_data->expirations = expirations;
  timer_data->timerspec = ts;

  task_t *task = malloc(sizeof(task_t));
  assert(task != NULL, "Error while trying to alloc watcher");

  task->on_complete = (*env)->NewGlobalRef(env, on_complete);
  task->user_data = (void *) (long) timer_data;
  task->operation_type = TIMER_OP;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&reactor->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  sqe->user_data = (uint64_t) task;

  io_uring_prep_read(sqe, tfd, expirations, sizeof(uint64_t), 0);

  assert(io_uring_submit(&reactor->queue) != -1, "Error while trying to io_uring_submit");

  (*env)->DeleteLocalRef(env, on_complete);

  return (ptr_t) task;
}

JNIEXPORT void JNICALL Java_Reactor_free_1timer(JNIEnv *env, jobject _this, ptr_t timer) {
  printf("[C] Java_Reactor_free_1timer\n");
  task_t *timer_task = (task_t*) timer;

  timer_data_t *timer_data = timer_task->user_data;

  close(timer_data->tfd);
  free(timer_data->expirations);
  free(timer_data->timerspec);
  free(timer_data);

  (*env)->DeleteGlobalRef(env, timer_task->on_complete);

  free(timer_task);
}

// Files FD

JNIEXPORT fd_t JNICALL Java_Reactor_open(JNIEnv* env, jobject _this, jstring jpath, jint flags) {
  printf("[C] Java_Reactor_open\n");
  const char* path = (*env)->GetStringUTFChars(env, jpath, NULL);
  assert(path != NULL, "Error trying to get UTFChars from file path");

  int fd = open(path, flags);

  (*env)->ReleaseStringUTFChars(env, jpath, path);
  assert(fd >= 0, "Error while trying to call open");

  return (fd_t)fd;
}

JNIEXPORT void JNICALL Java_Reactor_close(JNIEnv* _env, jobject _this, fd_t fd) {
  printf("[C] Java_Reactor_close\n");
  int e = close(fd);
  assert(e != -1, "Error trying to close fd");
}

JNIEXPORT ptr_t JNICALL Java_Reactor_file_1read(
  JNIEnv* env, jobject _this, ptr_t reactor_ptr, fd_t fd, jint length, jint offset, jobject on_complete
) {
  printf("[C] Java_Reactor_file_1read\n");
  reactor_t* reactor = (reactor_t*) reactor_ptr;

  jclass bytebuffer_class = (*env)->FindClass(env, "java/nio/ByteBuffer");
  assert(bytebuffer_class != NULL, "Error while trying to find ByteBuffer class");

  jmethodID alloc_direct_id = (*env)->GetStaticMethodID(env, bytebuffer_class, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
  assert(alloc_direct_id != NULL, "Error while trying to find alloc_direct_id method id");

  jobject jbuffer = (*env)->CallStaticObjectMethod(env, bytebuffer_class, alloc_direct_id, length);

  void* buffer = (*env)->GetDirectBufferAddress(env, jbuffer);

  task_t* task = malloc(sizeof(task_t));
  assert(task != NULL, "Error while trying to alloc watcher");

  struct io_uring_sqe* sqe = io_uring_get_sqe(&reactor->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  task->on_complete = (*env)->NewGlobalRef(env, on_complete);
  task->user_data = (*env)->NewGlobalRef(env, jbuffer);
  task->operation_type = READ_FILE_OP;

  io_uring_prep_read(sqe, fd, buffer, length, offset);

  sqe->user_data = (uint64_t) task;
  int e = io_uring_submit(&reactor->queue);
  assert(e != -1, "Error while trying to io_uring_submit");

  (*env)->DeleteLocalRef(env, bytebuffer_class);
  (*env)->DeleteLocalRef(env, jbuffer);
  (*env)->DeleteLocalRef(env, on_complete);

  return (ptr_t) task;
}

JNIEXPORT ptr_t JNICALL Java_Reactor_file_1write(
  JNIEnv* env, jobject _this, ptr_t reactor_ptr, fd_t fd, jobject jbuffer, jint length, jint offset, jobject on_complete
) {
  printf("[C] Java_Reactor_file_1write\n");
  reactor_t* reactor = (reactor_t*) reactor_ptr;

  void* buffer_ptr = (*env)->GetDirectBufferAddress(env, jbuffer);
  assert(buffer_ptr != NULL, "Error trying to get Direct Buffer address");

  task_t* task = malloc(sizeof(task_t));
  assert(task != NULL, "Error while trying to alloc task");

  task->on_complete = (*env)->NewGlobalRef(env, on_complete);
  task->user_data = (*env)->NewGlobalRef(env, jbuffer);
  task->operation_type = WRITE_FILE_OP;

  struct io_uring_sqe* sqe = io_uring_get_sqe(&reactor->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  io_uring_prep_write(sqe, fd, buffer_ptr, length, offset);

  sqe->user_data = (uint64_t) task;

  assert(io_uring_submit(&reactor->queue) != -1, "Error while trying to io_uring_submit");

  (*env)->DeleteLocalRef(env, on_complete);
  (*env)->DeleteLocalRef(env, jbuffer);

  return (ptr_t) task;
}

// Runners

void reactor_run_read_write(JNIEnv* env, task_t* task, struct io_uring_cqe* cqe) {
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
}

void reactor_run_event(JNIEnv* env, task_t* task) {
  fd_t efd = (fd_t) (long) task->user_data;

  uint64_t value = 0;
  assert(read(efd, &value, sizeof(value)) != -1, "Error while trying to read eventfd");

  jclass callback_class = (*env)->GetObjectClass(env, task->on_complete);
  assert(callback_class != NULL, "Error while trying to get callback class");

  jmethodID callback_id = (*env)->GetMethodID(env, callback_class, "accept", "(J)V");
  assert(callback_id != NULL, "Error while trying to get callback method id");

  (*env)->CallVoidMethod(env, task->on_complete, callback_id, value);

  (*env)->DeleteGlobalRef(env, task->on_complete);
  (*env)->DeleteLocalRef(env, callback_class);

  assert(close(efd) != -1, "Error while trying to close eventfd");

  free(task);
}

void reactor_run_timer(JNIEnv* env, reactor_t *reactor, task_t* task) {
  timer_data_t *timer_data = (timer_data_t*) task->user_data;

  jclass callback_class = (*env)->GetObjectClass(env, task->on_complete);
  assert(callback_class != NULL, "Error while trying to get callback class");

  jmethodID callback_id = (*env)->GetMethodID(env, callback_class, "run", "()V");
  assert(callback_id != NULL, "Error while trying to get callback method id");

  for(uint64_t i = 1; i <= *timer_data->expirations; i++)
    (*env)->CallVoidMethod(env, task->on_complete, callback_id);

  (*env)->DeleteLocalRef(env, callback_class);

  if (timer_data->timerspec->it_interval.tv_sec == 0 && timer_data->timerspec->it_interval.tv_nsec == 0) {
    Java_Reactor_free_1timer(env, NULL, (ptr_t) task);
    return;
  }

  struct io_uring_sqe *sqe = io_uring_get_sqe(&reactor->queue);
  assert(sqe != NULL, "Error while trying to get submission queue entry");

  sqe->user_data = (uint64_t) task;

  io_uring_prep_read(sqe, timer_data->tfd, timer_data->expirations, sizeof(uint64_t), 0);

  assert(io_uring_submit(&reactor->queue) != -1, "Error while trying to io_uring_submit");
}

JNIEXPORT void JNICALL Java_Reactor_reactor_1run(JNIEnv* env, jobject _this, ptr_t reactor_ptr, jint timeout) {
  printf("[C] Java_Reactor_reactor_1run\n");
  reactor_t* reactor = (reactor_t*) reactor_ptr;
  struct io_uring_cqe* cqe;

  if (timeout >= 0) {
    struct __kernel_timespec ts = {};
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000000;

    int e = io_uring_wait_cqe_timeout(&reactor->queue, &cqe, &ts);

    if (e == -ETIME) return;
    assert(e >= 0, "Error while trying to io_uring_wait_cqe_timeout");
  } else {
    int e = io_uring_wait_cqe(&reactor->queue, &cqe);

    if (e == -ETIME) return;
    assert(e >= 0, "Error while trying to io_uring_wait_cqe");
  }

  task_t* task = (task_t*)cqe->user_data;
  assert(cqe->res >= 0, "Error returned by I/O operation");

  printf("[C] Task received: %d\n", task->operation_type);

  switch (task->operation_type) {
    case READ_FILE_OP:
    case WRITE_FILE_OP:
      reactor_run_read_write(env, task, cqe);
      break;
    case EVENT_OP:
      reactor_run_event(env, task);
      break;
    case TIMER_OP:
      reactor_run_timer(env, reactor, task);
      break;
    default:
      assert(false, "Unkown operation being executed");
  }

  io_uring_cqe_seen(&reactor->queue, cqe);
}
