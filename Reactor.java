import java.util.function.BiConsumer;
import java.util.Random;

public class Reactor {
    public static final int EPOLLIN  = 0x001;
    public static final int EPOLLOUT = 0x004;

    public native long create_reactor(int max_events);
    public native void free_reactor(long reactor_ptr);
    public native void reactor_run(long reactor_ptr, int timeout);

    public native long create_watcher(long reactor_ptr, int fd, int events, BiConsumer<Integer, Integer> callback);
    public native void free_watcher(long watcher);

    public native int create_eventfd();
    public native void free_eventfd(int eventfd);
    public native void inc_eventfd(int eventfd);
    public native long read_eventfd(int eventfd);

    static {
        System.loadLibrary("reactor");
    }

    public static void main(String[] args) {
        Reactor reactor = new Reactor();
        Random rand = new Random();

        long reactor_ptr = reactor.create_reactor(16);
        int efd1 = reactor.create_eventfd();
        BiConsumer<Integer, Integer> wcb1 = (fd, events) -> {
            long value = reactor.read_eventfd(fd);
            System.out.println("Value of ping - 1: " + value);
        };
        long w1 = reactor.create_watcher(reactor_ptr, efd1, Reactor.EPOLLIN, wcb1);

        int efd2 = reactor.create_eventfd();
        BiConsumer<Integer, Integer> wcb2 = (fd, events) -> {
            long value = reactor.read_eventfd(fd);
            System.out.println("Value of ping - 2: " + value);
        };
        long w2 = reactor.create_watcher(reactor_ptr, efd2, Reactor.EPOLLIN, wcb2);

        Thread tt1 = new Thread(() -> {
                try {
                    while (true) {
                        Thread.sleep(1200 + rand.nextInt(1000));
                        reactor.inc_eventfd(efd1);
                        System.out.println("[Background - 1] incremented eventfd");
                    }
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
        });

        tt1.start();

        Thread tt2 = new Thread(() -> {
                try {
                    while (true) {
                        Thread.sleep(1200 + rand.nextInt(1000));
                        reactor.inc_eventfd(efd2);
                        System.out.println("[Background - 2] incremented eventfd");
                    }
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
        });

        tt2.start();

        System.out.println("[Main] Starting reactor event loop");
        while (true) {
            reactor.reactor_run(reactor_ptr, 1000);
        }
    }
}
