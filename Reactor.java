import java.nio.ByteBuffer;
import java.util.function.BiConsumer;
import java.util.function.LongConsumer;

public class Reactor {
    public static final int RDONLY = 0x0000;
    public static final int WRONLY = 0x0001;
    public static final int RDWR   = 0x0002;

    public static final int EFD_SEMAPHORE = 0x00000001;
    public static final int EFD_CLOEXEC   = 0x02000000;
    public static final int EFD_NONBLOCK  = 0x00004000;

    public static final int CLOCK_REALTIME           = 0x0000;
    public static final int CLOCK_MONOTONIC          = 0x0001;
    public static final int CLOCK_PROCESS_CPUTIME_ID = 0x0002;
    public static final int CLOCK_THREAD_CPUTIME_ID  = 0x0003;
    public static final int CLOCK_MONOTONIC_RAW      = 0x0004;
    public static final int CLOCK_REALTIME_COARSE    = 0x0005;
    public static final int CLOCK_MONOTONIC_COARSE   = 0x0006;
    public static final int CLOCK_BOOTTIME           = 0x0007;
    public static final int CLOCK_REALTIME_ALARM     = 0x0008;
    public static final int CLOCK_BOOTTIME_ALARM     = 0x0009;
    public static final int CLOCK_TAI                = 0x0011;

    public static final int TFD_NONBLOCK = 0x2;
    public static final int TFD_CLOEXEC  = 0x200000;

    public static final int TFD_TIMER_RLTVTIME = 0x0;
    public static final int TFD_TIMER_ABSTIME = 0x1;
    public static final int TFD_TIMER_CANCEL_ON_SET = 0x2;

    public static native long create_reactor(int ring_size);
    public static native void free_reactor(long reactor_ptr);
    public static native void reactor_run(long reactor_ptr, int timeout);

    public static native int open(String filepath, int flags);
    public static native void close(int fd);
    public static native long file_read(long reactor_ptr, int fd, int length, int offset, BiConsumer<ByteBuffer, Integer> callbackBiConsumer);
    public static native long file_write(long reactor_ptr, int fd, ByteBuffer buffer, int length, int offset,
                                  BiConsumer<ByteBuffer, Integer> callbackBiConsumer);

    public static native int create_eventfd(int flags);
    public static native void trigger_eventfd(int eventfd, long value);
    public static native void listen_eventfd(long reactor_ptr, int eventfd, LongConsumer callback);

    public static native long create_timer(long reactor_ptr, int value, int interval, int clock_type,
                                    int fd_flags, int timer_flags, Runnable callback);
    public static native void free_timer(long timer);

    static {
        System.loadLibrary("reactor");
    }
}
