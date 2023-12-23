#ifndef __LOG_H_
#define __LOG_H_

#define __LOCK_LOG_OUTPUT_

#ifndef __LOCK_LOG_OUTPUT_
#define WRITE_LOG(str, ...)                                                         \
    {                                                                               \
        spin_lock_acquire_no_log(&log_lock);                                        \
        printl("> [%s:%d core:%d] ", __FUNCTION__, __LINE__, get_current_cpu_id()); \
        printl(str, ##__VA_ARGS__);                                                 \
        spin_lock_release_no_log(&log_lock);                                        \
    }
#define PRINTKL(str, ...)                    \
    {                                        \
        spin_lock_acquire_no_log(&log_lock); \
        printk(str, ##__VA_ARGS__);          \
        printl(str, ##__VA_ARGS__);          \
        spin_lock_release_no_log(&log_lock); \
    }
#else
#define WRITE_LOG(str, ...)                                                         \
    {                                                                               \
        printl("> [%s:%d core:%d] ", __FUNCTION__, __LINE__, get_current_cpu_id()); \
        printl(str, ##__VA_ARGS__);                                                 \
    }
#define PRINTKL(str, ...)                    \
    {                                        \
        printk(str, ##__VA_ARGS__);          \
        printl(str, ##__VA_ARGS__);          \
    }
#endif // __LOCK_LOG_OUTPUT

#endif