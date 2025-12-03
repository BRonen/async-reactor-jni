import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
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

    public native long create_reactor(int ring_size);
    public native void free_reactor(long reactor_ptr);
    public native void reactor_run(long reactor_ptr, int timeout);

    public native int open(String filepath, int flags);
    public native void close(int fd);
    public native long file_read(long reactor_ptr, int fd, int length, int offset, BiConsumer<ByteBuffer, Integer> callbackBiConsumer);
    public native long file_write(long reactor_ptr, int fd, ByteBuffer buffer, int length, int offset,
                                  BiConsumer<ByteBuffer, Integer> callbackBiConsumer);

    public native int create_eventfd(int flags);
    public native void trigger_eventfd(int eventfd, long value);
    public native void listen_eventfd(long reactor_ptr, int eventfd, LongConsumer callback);

    public native long create_timer(long reactor_ptr, int value, int interval, int clock_type,
                                    int fd_flags, int timer_flags, Runnable callback);
    public native void free_timer(long timer);

    static {
        System.loadLibrary("reactor");
    }

    public static void main(String[] args) {
        Reactor r = new Reactor();

        long reactor_ptr = r.create_reactor(16);

        Runnable timeoutCallback = () -> {
            System.out.println("[Java] Timer achieved");
        };

        long timer = r.create_timer(reactor_ptr, 3, 1, CLOCK_REALTIME, 0, TFD_TIMER_RLTVTIME, timeoutCallback);

        r.reactor_run(reactor_ptr, 1000);
        r.reactor_run(reactor_ptr, 4000);
        r.reactor_run(reactor_ptr, 2000);

        try {
            Thread.sleep(4000);
        } catch (InterruptedException e) { }

        r.reactor_run(reactor_ptr, 8000);
        r.reactor_run(reactor_ptr, 4000);

        r.free_timer(timer);

        // int fd = r.open("./test.txt", Reactor.RDWR);

        // BiConsumer<ByteBuffer, Integer> callback0 = (buffer, length) -> {
        //     byte[] bytes = new byte[length];
        //     buffer.get(bytes);

        //     String value = new String(bytes, StandardCharsets.UTF_8);
        //     System.out.println("[Java] acceptor 0: " + value);
        // };

        // BiConsumer<ByteBuffer, Integer> callback1 = (buffer, length) -> {
        //     byte[] bytes = new byte[length];
        //     buffer.get(bytes);

        //     String value = new String(bytes, StandardCharsets.UTF_8);
        //     System.out.println("[Java] acceptor 1: " + value);
        //     r.file_read(reactor_ptr, fd, 16, 0, callback0);
        // };

        // BiConsumer<ByteBuffer, Integer> callback2 = (buffer, length) -> {
        //     byte[] bytes = new byte[length];
        //     buffer.get(bytes);

        //     String value = new String(bytes, StandardCharsets.UTF_8);
        //     System.out.println("[Java] acceptor 2: " + value);

        //     String input = "teste 123 alguma coisa";
        //     byte[] newBytes = input.getBytes(StandardCharsets.UTF_8);

        //     ByteBuffer buff = ByteBuffer.allocateDirect(newBytes.length);
        //     buff.put(newBytes);
        //     buff.flip();

        //     r.file_write(reactor_ptr, fd, buff, 16, 0, callback1);
        // };

        // r.file_read(reactor_ptr, fd, 16, 0, callback2);

        // r.reactor_run(reactor_ptr, 2000);
        // r.reactor_run(reactor_ptr, 2000);
        // r.reactor_run(reactor_ptr, 2000);

        // int efd = r.create_eventfd();

        // r.listen_eventfd(reactor_ptr, efd, (value) -> { System.out.println("[Java] event listener: " + value); });

        // r.reactor_run(reactor_ptr, 2000);

        // r.trigger_eventfd(efd, 200);

        // r.reactor_run(reactor_ptr, 2000);

        // r.close(fd);
        r.free_reactor(reactor_ptr);
    }
}
