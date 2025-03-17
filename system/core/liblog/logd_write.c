/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <time.h>
#include <stdio.h>
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <cutils/logger.h>
#include <cutils/logd.h>
#include <cutils/log.h>

#define LOG_BUF_SIZE	1024

#if FAKE_LOG_DEVICE
// This will be defined when building for the host.
#define log_open(pathname, flags) fakeLogOpen(pathname, flags)
#define log_writev(filedes, vector, count) fakeLogWritev(filedes, vector, count)
#define log_close(filedes) fakeLogClose(filedes)
#else
#define log_open(pathname, flags) open(pathname, flags)
#define log_writev(filedes, vector, count) writev(filedes, vector, count)
#define log_close(filedes) close(filedes)
#endif

static int __write_to_log_init(log_id_t, struct iovec *vec, size_t nr);
static int (*write_to_log)(log_id_t, struct iovec *vec, size_t nr) = __write_to_log_init;
#ifdef HAVE_PTHREADS
static pthread_mutex_t log_init_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static int log_fds[(int)LOG_ID_MAX] = { -1, -1, -1, -1 };

/*
 * This is used by the C++ code to decide if it should write logs through
 * the C code.  Basically, if /dev/log/... is available, we're running in
 * the simulator rather than a desktop tool and want to use the device.
 */
static enum {
    kLogUninitialized, kLogNotAvailable, kLogAvailable 
} g_log_status = kLogUninitialized;
int __android_log_dev_available(void)
{
    if (g_log_status == kLogUninitialized) {
        if (access("/dev/"LOGGER_LOG_MAIN, W_OK) == 0)
            g_log_status = kLogAvailable;
        else
            g_log_status = kLogNotAvailable;
    }

    return (g_log_status == kLogAvailable);
}

static int __write_to_log_null(log_id_t log_fd, struct iovec *vec, size_t nr)
{
    return -1;
}

static int __write_to_log_kernel(log_id_t log_id, struct iovec *vec, size_t nr)
{
    ssize_t ret;
    int log_fd;
    /* 通过 log_id 在全局数组 log_fds 中找到对应的日志设备文件描述符. */
    if (/*(int)log_id >= 0 &&*/ (int)log_id < (int)LOG_ID_MAX) {
        log_fd = log_fds[(int)log_id];
    } else {
        return EBADF;
    }

    do {
        /* 调用 log_writev(writev) 将日志写入设备. */
        ret = log_writev(log_fd, vec, nr);
        /* 如果 ret < 0 && errno == EINTR 则表示被信号中断，需要重新写入. */
    } while (ret < 0 && errno == EINTR);

    return ret;
}

static int __write_to_log_init(log_id_t log_id, struct iovec *vec, size_t nr)
{
#ifdef HAVE_PTHREADS
    pthread_mutex_lock(&log_init_lock);
#endif
    /* 判断 write_to_log 是否等于自身，从而决定是否打开 log 设备. */
    if (write_to_log == __write_to_log_init) {
        /* log_open 实际调用 open. */
        log_fds[LOG_ID_MAIN] = log_open("/dev/"LOGGER_LOG_MAIN, O_WRONLY);
        log_fds[LOG_ID_RADIO] = log_open("/dev/"LOGGER_LOG_RADIO, O_WRONLY);
        log_fds[LOG_ID_EVENTS] = log_open("/dev/"LOGGER_LOG_EVENTS, O_WRONLY);
        log_fds[LOG_ID_SYSTEM] = log_open("/dev/"LOGGER_LOG_SYSTEM, O_WRONLY);
        /* 将 write_to_log 指向 __write_to_log_kernel. */
        write_to_log = __write_to_log_kernel;

        if (log_fds[LOG_ID_MAIN] < 0 || log_fds[LOG_ID_RADIO] < 0 ||
                log_fds[LOG_ID_EVENTS] < 0) {
            log_close(log_fds[LOG_ID_MAIN]);
            log_close(log_fds[LOG_ID_RADIO]);
            log_close(log_fds[LOG_ID_EVENTS]);
            log_fds[LOG_ID_MAIN] = -1;
            log_fds[LOG_ID_RADIO] = -1;
            log_fds[LOG_ID_EVENTS] = -1;
            /* 如果打开设备失败，则将 write_to_log 指向 __write_to_log_null. */
            write_to_log = __write_to_log_null;
        }

        if (log_fds[LOG_ID_SYSTEM] < 0) {
            log_fds[LOG_ID_SYSTEM] = log_fds[LOG_ID_MAIN];
        }
    }

#ifdef HAVE_PTHREADS
    pthread_mutex_unlock(&log_init_lock);
#endif

    return write_to_log(log_id, vec, nr);
}

int __android_log_write(int prio, const char *tag, const char *msg)
{
    struct iovec vec[3];
    log_id_t log_id = LOG_ID_MAIN;

    if (!tag)
        tag = "";

    /* XXX: This needs to go! */
    if (!strcmp(tag, "HTC_RIL") ||
        !strncmp(tag, "RIL", 3) || /* Any log tag with "RIL" as the prefix */
        !strcmp(tag, "AT") ||
        !strcmp(tag, "GSM") ||
        !strcmp(tag, "STK") ||
        !strcmp(tag, "CDMA") ||
        !strcmp(tag, "PHONE") ||
        !strcmp(tag, "SMS")) /* 如果 tag 满足这些条件，则是 radio 日志记录. */
            log_id = LOG_ID_RADIO;

    vec[0].iov_base   = (unsigned char *) &prio; /* 优先级 */
    vec[0].iov_len    = 1;
    vec[1].iov_base   = (void *) tag;    /* 标签 */
    vec[1].iov_len    = strlen(tag) + 1; /* 标签长度 + '\0' */
    vec[2].iov_base   = (void *) msg;    /* 消息 */
    vec[2].iov_len    = strlen(msg) + 1; /* 消息长度 + '\0' */

    return write_to_log(log_id, vec, 3);
}

int __android_log_buf_write(int bufID, int prio, const char *tag, const char *msg)
{
    struct iovec vec[3];

    if (!tag)
        tag = "";

    /* XXX: This needs to go! */
    if (!strcmp(tag, "HTC_RIL") ||
        !strncmp(tag, "RIL", 3) || /* Any log tag with "RIL" as the prefix */
        !strcmp(tag, "AT") ||
        !strcmp(tag, "GSM") ||
        !strcmp(tag, "STK") ||
        !strcmp(tag, "CDMA") ||
        !strcmp(tag, "PHONE") ||
        !strcmp(tag, "SMS")) /* 如果 tag 满足这些条件，则是 radio 日志记录. */
            bufID = LOG_ID_RADIO;

    vec[0].iov_base   = (unsigned char *) &prio; /* 优先级 */
    vec[0].iov_len    = 1;
    vec[1].iov_base   = (void *) tag;    /* 标签 */
    vec[1].iov_len    = strlen(tag) + 1; /* 标签长度 + '\0' */
    vec[2].iov_base   = (void *) msg;    /* 消息 */
    vec[2].iov_len    = strlen(msg) + 1; /* 消息长度 + '\0' */

    return write_to_log(bufID, vec, 3);
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap)
{
    char buf[LOG_BUF_SIZE];    
    /* 格式化日志内容. */
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    /* 写入日志记录. */
    return __android_log_write(prio, tag, buf);
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    return __android_log_write(prio, tag, buf);
}

int __android_log_buf_print(int bufID, int prio, const char *tag, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    return __android_log_buf_write(bufID, prio, tag, buf);
}

void __android_log_assert(const char *cond, const char *tag,
			  const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];    

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    __android_log_write(ANDROID_LOG_FATAL, tag, buf);

    __builtin_trap(); /* trap so we have a chance to debug the situation */
}

/* 写入的日志类型为 events. */
int __android_log_bwrite(int32_t tag, const void *payload, size_t len)
{
    struct iovec vec[2];

    vec[0].iov_base = &tag;
    vec[0].iov_len = sizeof(tag);
    vec[1].iov_base = (void*)payload;
    vec[1].iov_len = len;

    return write_to_log(LOG_ID_EVENTS, vec, 2);
}

/*
 * Like __android_log_bwrite, but takes the type as well.  Doesn't work
 * for the general case where we're generating lists of stuff, but very
 * handy if we just want to dump an integer into the log.
 */
int __android_log_btwrite(int32_t tag, char type, const void *payload,
    size_t len)
{
    struct iovec vec[3];

    vec[0].iov_base = &tag;
    vec[0].iov_len = sizeof(tag);
    vec[1].iov_base = &type;
    vec[1].iov_len = sizeof(type);
    vec[2].iov_base = (void*)payload;
    vec[2].iov_len = len;

    return write_to_log(LOG_ID_EVENTS, vec, 3);
}
