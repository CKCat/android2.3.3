// Copyright 2006 The Android Open Source Project

#include <cutils/logger.h>
#include <cutils/logd.h>
#include <cutils/sockets.h>
#include <cutils/logprint.h>
#include <cutils/event_tag_map.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define DEFAULT_LOG_ROTATE_SIZE_KBYTES 16
#define DEFAULT_MAX_ROTATED_LOGS 4

static AndroidLogFormat * g_logformat;
static bool g_nonblock = false;
static int g_tail_lines = 0;

/* logd prefixes records with a length field */
#define RECORD_LENGTH_FIELD_SIZE_BYTES sizeof(uint32_t)

#define LOG_FILE_DIR    "/dev/log/"

/* 日志记录队列结构体.每一种类型的日志记录都对应有一个日志记录队列. */
struct queued_entry_t {
    union { /* 日志内容 */
        unsigned char buf[LOGGER_ENTRY_MAX_LEN + 1] __attribute__((aligned(4)));
        struct logger_entry entry __attribute__((aligned(4)));
    };
    queued_entry_t* next; /* 下一个日志记录队列 */ 

    queued_entry_t() {
        next = NULL;
    }
};

static int cmp(queued_entry_t* a, queued_entry_t* b) {
    int n = a->entry.sec - b->entry.sec;
    if (n != 0) {
        return n;
    }
    return a->entry.nsec - b->entry.nsec;
}

/* 日志设备. */
struct log_device_t {
    char* device; /* 日志设备名称. /dev/log/xxx */
    bool binary;  /* 是否是二进制日志. */
    int fd;       /* 日志设备文件描述符. */  
    bool printed; /* 是否已经是输出状态. */
    char label;   /* 日志设备的标号，分别为 m,s,r,e*/

    queued_entry_t* queue; /* 日志记录队列. */
    log_device_t* next; /* 连接下一个日志设备. */

    log_device_t(char* d, bool b, char l) {
        device = d;
        binary = b;
        label = l;
        queue = NULL;
        next = NULL;
        printed = false;
    }

    /* 将一条日志添加到内部的日志记录队列中. */
    void enqueue(queued_entry_t* entry) {
        if (this->queue == NULL) {
            this->queue = entry;
        } else {
            queued_entry_t** e = &this->queue;
            while (*e && cmp(entry, *e) >= 0) {/* 按照时间顺序插入. */ 
                e = &((*e)->next);
            }
            entry->next = *e;
            *e = entry;
        }
    }
};

namespace android {

/* Global Variables */

static const char * g_outputFileName = NULL;
static int g_logRotateSizeKBytes = 0;                   // 0 means "no log rotation"
static int g_maxRotatedLogs = DEFAULT_MAX_ROTATED_LOGS; // 0 means "unbounded"
static int g_outFD = -1;
static off_t g_outByteCount = 0;
static int g_printBinary = 0;
static int g_devCount = 0;

static EventTagMap* g_eventTagMap = NULL;

static int openLogFile (const char *pathname)
{
    return open(g_outputFileName, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
}

static void rotateLogs()
{
    int err;

    // Can't rotate logs if we're not outputting to a file
    if (g_outputFileName == NULL) {
        return;
    }

    close(g_outFD);

    for (int i = g_maxRotatedLogs ; i > 0 ; i--) {
        char *file0, *file1;

        asprintf(&file1, "%s.%d", g_outputFileName, i);

        if (i - 1 == 0) {
            asprintf(&file0, "%s", g_outputFileName);
        } else {
            asprintf(&file0, "%s.%d", g_outputFileName, i - 1);
        }

        err = rename (file0, file1);

        if (err < 0 && errno != ENOENT) {
            perror("while rotating log files");
        }

        free(file1);
        free(file0);
    }

    g_outFD = openLogFile (g_outputFileName);

    if (g_outFD < 0) {
        perror ("couldn't open output file");
        exit(-1);
    }

    g_outByteCount = 0;

}

void printBinary(struct logger_entry *buf)
{
    size_t size = sizeof(logger_entry) + buf->len;
    int ret;
    
    do {
        /* 直接使用 write 输出到文件或标准输出. */
        ret = write(g_outFD, buf, size);
    } while (ret < 0 && errno == EINTR);
}

static void processBuffer(log_device_t* dev, struct logger_entry *buf)
{
    int bytesWritten = 0;
    int err;
    AndroidLogEntry entry;
    char binaryMsgBuf[1024];
    /* 将内容转换为 AndroidLogEntry 结构体. */
    if (dev->binary) {/* 如果是 evnents 日志. */
        err = android_log_processBinaryLogBuffer(buf, &entry, g_eventTagMap,
                binaryMsgBuf, sizeof(binaryMsgBuf));
        //printf(">>> pri=%d len=%d msg='%s'\n",
        //    entry.priority, entry.messageLen, entry.message);
    } else { /* 其他日志 */
        err = android_log_processLogBuffer(buf, &entry);
    }
    if (err < 0) {
        goto error;
    }
    /* 根据标签判断是否输出.*/
    if (android_log_shouldPrintLine(g_logformat, entry.tag, entry.priority)) {
        if (false && g_devCount > 1) {
            binaryMsgBuf[0] = dev->label;
            binaryMsgBuf[1] = ' ';
            bytesWritten = write(g_outFD, binaryMsgBuf, 2);
            if (bytesWritten < 0) {
                perror("output error");
                exit(-1);
            }
        }
        /* 输出日志到文件或标准输出. */
        bytesWritten = android_log_printLogLine(g_logformat, g_outFD, &entry);

        if (bytesWritten < 0) {
            perror("output error");
            exit(-1);
        }
    }

    g_outByteCount += bytesWritten;

    if (g_logRotateSizeKBytes > 0 
        && (g_outByteCount / 1024) >= g_logRotateSizeKBytes
    ) {
        /* 设置下一个日志输出文件. */
        rotateLogs();
    }

error:
    //fprintf (stderr, "Error processing record\n");
    return;
}

static void chooseFirst(log_device_t* dev, log_device_t** firstdev) {
    for (*firstdev = NULL; dev != NULL; dev = dev->next) {
        if (dev->queue != NULL && (*firstdev == NULL || cmp(dev->queue, (*firstdev)->queue) < 0)) {
            *firstdev = dev;
        }
    }
}

static void maybePrintStart(log_device_t* dev) {
    if (!dev->printed) {
        dev->printed = true;
        if (g_devCount > 1 && !g_printBinary) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "--------- beginning of %s\n", dev->device);
            if (write(g_outFD, buf, strlen(buf)) < 0) {
                perror("output error");
                exit(-1);
            }
        }
    }
}

static void skipNextEntry(log_device_t* dev) {
    maybePrintStart(dev);
    queued_entry_t* entry = dev->queue;
    dev->queue = entry->next;
    delete entry;
}

static void printNextEntry(log_device_t* dev) {
    maybePrintStart(dev);
    if (g_printBinary) {
        printBinary(&dev->queue->entry);
    } else {
        processBuffer(dev, &dev->queue->entry);
    }
    skipNextEntry(dev);
}

static void readLogLines(log_device_t* devices)
{
    log_device_t* dev;
    int max = 0;
    int ret;
    int queued_lines = 0;
    bool sleep = true;

    int result;
    fd_set readset;

    for (dev=devices; dev; dev = dev->next) {
        if (dev->fd > max) {
            max = dev->fd;
        }
    }

    while (1) {
        do {
            timeval timeout = { 0, 5000 /* 5ms */ }; // If we oversleep it's ok, i.e. ignore EINTR.
            FD_ZERO(&readset);
            /* readset 保存所有打开的日志设备文件描述符.*/
            for (dev=devices; dev; dev = dev->next) {
                FD_SET(dev->fd, &readset);
            }
            /* 监控是否有内容可读.*/
            result = select(max + 1, &readset, NULL, NULL, sleep ? NULL : &timeout);
        } while (result == -1 && errno == EINTR);/* 条件成立的话则表明有信号处理. 需要重新设置*/

        if (result >= 0) {
            /* 依次处理有新的日志可读的日志设备. */
            for (dev=devices; dev; dev = dev->next) {
                if (FD_ISSET(dev->fd, &readset)) {
                    /* 创建一个 queued_entry_t 对象. */
                    queued_entry_t* entry = new queued_entry_t();
                    /* NOTE: driver guarantees we read exactly one full entry */
                    /* 将日志读到结构体中. */
                    ret = read(dev->fd, entry->buf, LOGGER_ENTRY_MAX_LEN);
                    if (ret < 0) {/* 如果出错则退出程序.*/
                        if (errno == EINTR) { /* 被信号中断. */
                            delete entry;
                            goto next;
                        }
                        if (errno == EAGAIN) { /* 以非阻塞模式打开. */
                            delete entry;
                            break;
                        }
                        perror("logcat read");
                        exit(EXIT_FAILURE);
                    }
                    else if (!ret) {
                        fprintf(stderr, "read: Unexpected EOF!\n");
                        exit(EXIT_FAILURE);
                    }
                    /* 将读到的日志内容添加到日志记录队列中. */
                    entry->entry.msg[entry->entry.len] = '\0';
                    
                    dev->enqueue(entry);
                    ++queued_lines;
                }
            }

            if (result == 0) { /* 为真，则表明 select 超时了.*/
                // we did our short timeout trick and there's nothing new
                // print everything we have and wait for more data
                sleep = true;
                while (true) { /* 输出之前已经读出来的日志记录. */
                    /* 找到包含有最早未输出日志记录的日志设备. */
                    chooseFirst(devices, &dev);
                    if (dev == NULL) {
                        break;
                    }
                    if (g_tail_lines == 0 || queued_lines <= g_tail_lines) {
                        /* 输出日志记录. */
                        printNextEntry(dev);
                    } else {
                        skipNextEntry(dev);
                    }
                    --queued_lines;
                }

                // the caller requested to just dump the log and exit
                if (g_nonblock) {
                    exit(0);
                }
            } else { /* 可能还有新的日志可读. */
                // print all that aren't the last in their list
                sleep = false;
                while (g_tail_lines == 0 || queued_lines > g_tail_lines) {
                    
                    chooseFirst(devices, &dev);
                    if (dev == NULL || dev->queue->next == NULL) {
                        break;
                    }
                    if (g_tail_lines == 0) {
                        /* 如果没有设置可以输出最新日志的条数，则直接输出.*/
                        printNextEntry(dev);
                    } else {
                        skipNextEntry(dev);
                    }
                    /*  减少未输出的日志记录数，直到小于 g_tail_lines 
                        并且没有新的日志可读，则去 result == 0 处执行输出日志. */
                    --queued_lines;
                }
            }
        }
next:
        ;
    }
}

static int clearLog(int logfd)
{
    return ioctl(logfd, LOGGER_FLUSH_LOG);
}

/* returns the total size of the log's ring buffer */
static int getLogSize(int logfd)
{
    return ioctl(logfd, LOGGER_GET_LOG_BUF_SIZE);
}

/* returns the readable size of the log's ring buffer (that is, amount of the log consumed) */
static int getLogReadableSize(int logfd)
{
    return ioctl(logfd, LOGGER_GET_LOG_LEN);
}

static void setupOutput()
{
    /* g_outputFileName 为 NULL 则输出到标准输出. */
    if (g_outputFileName == NULL) {
        g_outFD = STDOUT_FILENO;

    } else {
        struct stat statbuf;
        /* 输出到 g_outputFileName 文件. */
        g_outFD = openLogFile (g_outputFileName);

        if (g_outFD < 0) {
            perror ("couldn't open output file");
            exit(-1);
        }

        fstat(g_outFD, &statbuf);

        g_outByteCount = statbuf.st_size;
    }
}

static void show_help(const char *cmd)
{
    fprintf(stderr,"Usage: %s [options] [filterspecs]\n", cmd);

    fprintf(stderr, "options include:\n"
                    "  -s              Set default filter to silent.\n"
                    "                  Like specifying filterspec '*:s'\n"
                    "  -f <filename>   Log to file. Default to stdout\n"
                    "  -r [<kbytes>]   Rotate log every kbytes. (16 if unspecified). Requires -f\n"
                    "  -n <count>      Sets max number of rotated logs to <count>, default 4\n"
                    "  -v <format>     Sets the log print format, where <format> is one of:\n\n"
                    "                  brief process tag thread raw time threadtime long\n\n"
                    "  -c              clear (flush) the entire log and exit\n"
                    "  -d              dump the log and then exit (don't block)\n"
                    "  -t <count>      print only the most recent <count> lines (implies -d)\n"
                    "  -g              get the size of the log's ring buffer and exit\n"
                    "  -b <buffer>     request alternate ring buffer\n"
                    "                  ('main' (default), 'radio', 'events')\n"
                    "  -B              output the log in binary");


    fprintf(stderr,"\nfilterspecs are a series of \n"
                   "  <tag>[:priority]\n\n"
                   "where <tag> is a log component tag (or * for all) and priority is:\n"
                   "  V    Verbose\n"
                   "  D    Debug\n"
                   "  I    Info\n"
                   "  W    Warn\n"
                   "  E    Error\n"
                   "  F    Fatal\n"
                   "  S    Silent (supress all output)\n"
                   "\n'*' means '*:d' and <tag> by itself means <tag>:v\n"
                   "\nIf not specified on the commandline, filterspec is set from ANDROID_LOG_TAGS.\n"
                   "If no filterspec is found, filter defaults to '*:I'\n"
                   "\nIf not specified with -v, format is set from ANDROID_PRINTF_LOG\n"
                   "or defaults to \"brief\"\n\n");



}


} /* namespace android */

static int setLogFormat(const char * formatString)
{
    static AndroidLogPrintFormat format;

    format = android_log_formatFromString(formatString);

    if (format == FORMAT_OFF) {
        // FORMAT_OFF means invalid string
        return -1;
    }

    android_log_setPrintFormat(g_logformat, format);

    return 0;
}

extern "C" void logprint_run_tests(void);

int main(int argc, char **argv)
{
    int err;
    int hasSetLogFormat = 0;
    int clearLog = 0;
    int getLogSize = 0;
    int mode = O_RDONLY;
    const char *forceFilters = NULL;
    log_device_t* devices = NULL;
    log_device_t* dev;
    bool needBinary = false;
    /* 创建一个全局的日志记录输出格式和输出过滤器对象. */
    g_logformat = android_log_format_new();

    if (argc == 2 && 0 == strcmp(argv[1], "--test")) {
        logprint_run_tests();
        exit(0);
    }

    if (argc == 2 && 0 == strcmp(argv[1], "--help")) {
        android::show_help(argv[0]);
        exit(0);
    }

    for (;;) {
        int ret;
        /* 解析命令行参数. */
        ret = getopt(argc, argv, "cdt:gsQf:r::n:v:b:B");

        if (ret < 0) {
            break;
        }

        switch(ret) {
            case 's': /* 设置默认过滤规则 */
                // default to all silent
                android_log_addFilterRule(g_logformat, "*:s");
            break;

            case 'c': /* 设置清除日志标志 */
                clearLog = 1;
                mode = O_WRONLY;
            break;

            case 'd': /* 设置为不阻塞，当没有日志可读时直接退出. */
                g_nonblock = true;
            break;

            case 't': /* 设置只输出最新的日志记录的条数. */
                g_nonblock = true;
                g_tail_lines = atoi(optarg);
            break;

            case 'g': /* 设置获取日志大小标志 */
                getLogSize = 1;
            break;

            case 'b': { /* 初始化日志设备. */
                char* buf = (char*) malloc(strlen(LOG_FILE_DIR) + strlen(optarg) + 1);
                strcpy(buf, LOG_FILE_DIR);
                strcat(buf, optarg); /*根据 optarg 设置日志设备路径. */

                bool binary = strcmp(optarg, "events") == 0;
                if (binary) { /* 这里表示要解析 /system/etc/event-log-tags 文件. */
                    needBinary = true;
                }

                if (devices) {
                    dev = devices;
                    while (dev->next) {
                        dev = dev->next;
                    }
                    dev->next = new log_device_t(buf, binary, optarg[0]);
                } else {
                    devices = new log_device_t(buf, binary, optarg[0]);
                }
                android::g_devCount++; /* 记录 logcat 打开的日志设备的数量. */
            }
            break;

            case 'B': /* 设置二进制输出标志. */
                android::g_printBinary = 1;
            break;

            case 'f': /* 设置输出文件路径. */
                // redirect output to a file

                android::g_outputFileName = optarg;

            break;

            case 'r': /* 设置每个日志记录输出文件的最大容量. */
                if (optarg == NULL) {                
                    android::g_logRotateSizeKBytes 
                                = DEFAULT_LOG_ROTATE_SIZE_KBYTES;
                } else {
                    long logRotateSize;
                    char *lastDigit;

                    if (!isdigit(optarg[0])) {
                        fprintf(stderr,"Invalid parameter to -r\n");
                        android::show_help(argv[0]);
                        exit(-1);
                    }
                    android::g_logRotateSizeKBytes = atoi(optarg);
                }
            break;

            case 'n': /* 设置日志记录输出文件的最大个数.*/
                if (!isdigit(optarg[0])) {
                    fprintf(stderr,"Invalid parameter to -r\n");
                    android::show_help(argv[0]);
                    exit(-1);
                }

                android::g_maxRotatedLogs = atoi(optarg);
            break;

            case 'v': /* 设置日志格式 */
                err = setLogFormat (optarg);
                if (err < 0) {
                    fprintf(stderr,"Invalid parameter to -v\n");
                    android::show_help(argv[0]);
                    exit(-1);
                }

                hasSetLogFormat = 1;
            break;

            case 'Q': /* 检查内核命令行参数并设置强制过滤规则 */
                /* this is a *hidden* option used to start a version of logcat                 */
                /* in an emulated device only. it basically looks for androidboot.logcat=      */
                /* on the kernel command line. If something is found, it extracts a log filter */
                /* and uses it to run the program. If nothing is found, the program should     */
                /* quit immediately                                                            */
#define  KERNEL_OPTION  "androidboot.logcat="
#define  CONSOLE_OPTION "androidboot.console="
                {
                    int          fd;
                    char*        logcat;
                    char*        console;
                    int          force_exit = 1;
                    static char  cmdline[1024];

                    fd = open("/proc/cmdline", O_RDONLY);
                    if (fd >= 0) {
                        int  n = read(fd, cmdline, sizeof(cmdline)-1 );
                        if (n < 0) n = 0;
                        cmdline[n] = 0;
                        close(fd);
                    } else {
                        cmdline[0] = 0;
                    }

                    logcat  = strstr( cmdline, KERNEL_OPTION );
                    console = strstr( cmdline, CONSOLE_OPTION );
                    if (logcat != NULL) {
                        char*  p = logcat + sizeof(KERNEL_OPTION)-1;;
                        char*  q = strpbrk( p, " \t\n\r" );;

                        if (q != NULL)
                            *q = 0;

                        forceFilters = p;
                        force_exit   = 0;
                    }
                    /* if nothing found or invalid filters, exit quietly */
                    if (force_exit)
                        exit(0);

                    /* redirect our output to the emulator console */
                    if (console) {
                        char*  p = console + sizeof(CONSOLE_OPTION)-1;
                        char*  q = strpbrk( p, " \t\n\r" );
                        char   devname[64];
                        int    len;

                        if (q != NULL) {
                            len = q - p;
                        } else
                            len = strlen(p);

                        len = snprintf( devname, sizeof(devname), "/dev/%.*s", len, p );
                        fprintf(stderr, "logcat using %s (%d)\n", devname, len);
                        if (len < (int)sizeof(devname)) {
                            fd = open( devname, O_WRONLY );
                            if (fd >= 0) {
                                dup2(fd, 1);
                                dup2(fd, 2);
                                close(fd);
                            }
                        }
                    }
                }
                break;

            default:
                fprintf(stderr,"Unrecognized Option\n");
                android::show_help(argv[0]);
                exit(-1);
            break;
        }
    }

    if (!devices) {/* 默认情况下会打开 /dev/log/main */
        devices = new log_device_t(strdup("/dev/"LOGGER_LOG_MAIN), false, 'm');
        android::g_devCount = 1;
        int accessmode =
                  (mode & O_RDONLY) ? R_OK : 0
                | (mode & O_WRONLY) ? W_OK : 0;
        // only add this if it's available
        /* 如果 /dev/log/system 存在，也会打开它. */
        if (0 == access("/dev/"LOGGER_LOG_SYSTEM, accessmode)) {
            devices->next = new log_device_t(strdup("/dev/"LOGGER_LOG_SYSTEM), false, 's');
            android::g_devCount++;
        }
    }

    if (android::g_logRotateSizeKBytes != 0 
        && android::g_outputFileName == NULL
    ) {
        fprintf(stderr,"-r requires -f as well\n");
        android::show_help(argv[0]);
        exit(-1);
    }
    /* 设置日志记录默认输出到文件还是标准输出. */
    android::setupOutput();

    if (hasSetLogFormat == 0) {
        /* 没有指定输出格式，则通过环境变量 ANDROID_PRINTF_LOG 来设置. */
        const char* logFormat = getenv("ANDROID_PRINTF_LOG");

        if (logFormat != NULL) {
            err = setLogFormat(logFormat);

            if (err < 0) {
                fprintf(stderr, "invalid format in ANDROID_PRINTF_LOG '%s'\n", 
                                    logFormat);
            }
        }
    }
    /* 如果指定了 Q 参数，则设置 forceFilters 为 /proc/cmdline 中的日志输出过滤器.*/
    if (forceFilters) {
        /* 添加 forceFilters 到输出过滤器列表. */
        err = android_log_addFilterString(g_logformat, forceFilters);
        if (err < 0) {
            fprintf (stderr, "Invalid filter expression in -logcat option\n");
            exit(0);
        }
    } else if (argc == optind) { /* 判断是否指定了其他额外参数. */
        // Add from environment variable
        /* 如果没有指定，则使用环境变量 ANDROID_LOG_TAGS 的值作为过滤器. */
        char *env_tags_orig = getenv("ANDROID_LOG_TAGS");

        if (env_tags_orig != NULL) {
            err = android_log_addFilterString(g_logformat, env_tags_orig);

            if (err < 0) { 
                fprintf(stderr, "Invalid filter expression in" 
                                    " ANDROID_LOG_TAGS\n");
                android::show_help(argv[0]);
                exit(-1);
            }
        }
    } else { /* 如果指定了其他额外参数. */
        // Add from commandline
        /* 循环检查这些额外参数是否是日志记录输出过滤器.*/
        for (int i = optind ; i < argc ; i++) {
            err = android_log_addFilterString(g_logformat, argv[i]);

            if (err < 0) { 
                fprintf (stderr, "Invalid filter expression '%s'\n", argv[i]);
                android::show_help(argv[0]);
                exit(-1);
            }
        }
    }

    dev = devices;
    while (dev) { /* 打开所有的日志设备. */
        dev->fd = open(dev->device, mode);
        if (dev->fd < 0) {
            fprintf(stderr, "Unable to open log device '%s': %s\n",
                dev->device, strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (clearLog) {
            int ret;
            ret = android::clearLog(dev->fd);
            if (ret) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }
        }

        if (getLogSize) {
            int size, readable;

            size = android::getLogSize(dev->fd);
            if (size < 0) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }

            readable = android::getLogReadableSize(dev->fd);
            if (readable < 0) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }

            printf("%s: ring buffer is %dKb (%dKb consumed), "
                   "max entry is %db, max payload is %db\n", dev->device,
                   size / 1024, readable / 1024,
                   (int) LOGGER_ENTRY_MAX_LEN, (int) LOGGER_ENTRY_MAX_PAYLOAD);
        }

        dev = dev->next;
    }

    if (getLogSize) {
        return 0;
    }
    if (clearLog) {
        return 0;
    }

    //LOG_EVENT_INT(10, 12345);
    //LOG_EVENT_LONG(11, 0x1122334455667788LL);
    //LOG_EVENT_STRING(0, "whassup, doc?");

    if (needBinary)/* 如果包含二进制，则表明打开了 events 日志.*/
        android::g_eventTagMap = android_openEventTagMap(EVENT_TAG_MAP_FILE);
    /* 开始读取打开的日志设备的日志记录. */
    android::readLogLines(devices);

    return 0;
}
