import java.util.function.BiConsumer;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Random;

public class Reactor {
    public static final int EPOLLIN  = 0x001;
    public static final int EPOLLOUT = 0x004;

    public static final int RDONLY = 0x0000;
    public static final int WRONLY = 0x0001;
    public static final int RDWR   = 0x0002;

    public native long create_reactor(int ring_size);
    public native void free_reactor(long reactor_ptr);
    public native void reactor_run(long reactor_ptr, int timeout);

    public native int open(String filepath, int flags);
    public native void close(int fd);
    public native long file_read(long reactor_ptr, int fd, int length, int offset, BiConsumer<ByteBuffer, Integer> callbackBiConsumer);
    public native long file_write(long reactor_ptr, int fd, ByteBuffer buffer, int length, int offset,
                                  BiConsumer<ByteBuffer, Integer> callbackBiConsumer);

    // public native int create_eventfd();
    // public native void free_eventfd(int eventfd);
    // public native void inc_eventfd(int eventfd);
    // public native long read_eventfd(int eventfd);

    static {
        System.loadLibrary("reactor");
    }

    public static void main(String[] args) {
        Reactor r = new Reactor();
        // Random rand = new Random();

        long reactor_ptr = r.create_reactor(16);
        int fd = r.open("./test.txt", Reactor.RDWR);

        BiConsumer<ByteBuffer, Integer> callback0 = (buffer, length) -> {
            byte[] bytes = new byte[length];
            buffer.get(bytes);

            String value = new String(bytes, StandardCharsets.UTF_8);
            System.out.println("[Java] acceptor 0: " + value);
        };

        BiConsumer<ByteBuffer, Integer> callback1 = (buffer, length) -> {
            byte[] bytes = new byte[length];
            buffer.get(bytes);

            String value = new String(bytes, StandardCharsets.UTF_8);
            System.out.println("[Java] acceptor 1: " + value);
            r.file_read(reactor_ptr, fd, 16, 0, callback0);
        };

        BiConsumer<ByteBuffer, Integer> callback2 = (buffer, length) -> {
            byte[] bytes = new byte[length];
            buffer.get(bytes);

            String value = new String(bytes, StandardCharsets.UTF_8);
            System.out.println("[Java] acceptor 2: " + value);

            String input = "teste 123 alguma coisa";
            byte[] newBytes = input.getBytes(StandardCharsets.UTF_8);

            ByteBuffer buff = ByteBuffer.allocateDirect(newBytes.length);
            buff.put(newBytes);
            buff.flip();

            r.file_write(reactor_ptr, fd, buff, 16, 0, callback1);
        };

        r.file_read(reactor_ptr, fd, 16, 0, callback2);

        r.reactor_run(reactor_ptr, 0);
        r.reactor_run(reactor_ptr, 0);
        r.reactor_run(reactor_ptr, 0);
        r.reactor_run(reactor_ptr, 0);
        r.reactor_run(reactor_ptr, 0);
        r.reactor_run(reactor_ptr, 0);

        r.close(fd);
        r.free_reactor(reactor_ptr);

        // int efd1 = reactor.create_eventfd();
        // BiConsumer<Integer, Integer> wcb1 = (fd, events) -> {
        //     long value = reactor.read_eventfd(fd);
        //     System.out.println("Value of ping - 1: " + value);
        // };
        // long w1 = reactor.create_watcher(reactor_ptr, efd1, Reactor.EPOLLIN, wcb1);

        // int efd2 = reactor.create_eventfd();
        // BiConsumer<Integer, Integer> wcb2 = (fd, events) -> {
        //     long value = reactor.read_eventfd(fd);
        //     System.out.println("Value of ping - 2: " + value);
        // };
        // long w2 = reactor.create_watcher(reactor_ptr, efd2, Reactor.EPOLLIN, wcb2);

        // Thread tt1 = new Thread(() -> {
        //         try {
        //             while (true) {
        //                 Thread.sleep(1200 + rand.nextInt(1000));
        //                 reactor.inc_eventfd(efd1);
        //                 System.out.println("[Background - 1] incremented eventfd");
        //             }
        //         } catch (InterruptedException e) {
        //             throw new RuntimeException(e);
        //         }
        // });

        // tt1.start();

        // Thread tt2 = new Thread(() -> {
        //         try {
        //             while (true) {
        //                 Thread.sleep(1200 + rand.nextInt(1000));
        //                 reactor.inc_eventfd(efd2);
        //                 System.out.println("[Background - 2] incremented eventfd");
        //             }
        //         } catch (InterruptedException e) {
        //             throw new RuntimeException(e);
        //         }
        // });

        // tt2.start();

        // System.out.println("[Main] Starting reactor event loop");
        // while (true) {
        //     reactor.reactor_run(reactor_ptr, 1000);
        // }
    }
}
