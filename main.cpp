#include "mainwindow.h"

#include <QApplication>
#include <unistd.h>

#include <stdio.h>
#include <linux/gpio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>



#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#define DEV_PATH        "/dev/led_device0"

#define LED_IOC_MAGIC   'L'
#define LED1_IOC_ON     _IO(LED_IOC_MAGIC, 0)
#define LED1_IOC_OFF    _IO(LED_IOC_MAGIC, 1)

#define LED2_IOC_ON     _IO(LED_IOC_MAGIC, 3)
#define LED2_IOC_OFF    _IO(LED_IOC_MAGIC, 4)

MainWindow* MainWindow::instance = nullptr;
int led_blink_left = 1;
int led_blink_right = 1;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s read                     (read() trạng thái LED1: 0/1)\n"
            "  %s write on|off              (write() điều khiển LED1)\n"
            "  %s led1 on|off               (ioctl điều khiển LED1)\n"
            "  %s led2 on|off               (ioctl điều khiển LED2)\n"
            "  %s get <count>               (ioctl GET nhiều LED, vd count=2)\n",
            prog, prog, prog, prog, prog);

}

static int do_ioctl_simple(int fd, unsigned long req)
{
    if (ioctl(fd, req) < 0) {
        perror("ioctl");
        return -1;
    }
    return 0;
}

static int do_write_led1(int fd, const char *cmd)
{
    ssize_t n = write(fd, cmd, strlen(cmd));
    if (n < 0) {
        perror("write");
        return -1;
    }
    printf("Wrote \"%s\" to device\n", cmd);
    return 0;
}

void *read_button(void *arg)
{


    int fd_key = open("/dev/input/event0", O_RDONLY);

    if (fd_key < 0)
    {
        printf("Open file failed (error code: %d)\n", fd_key);
        return NULL;
    }
    struct pollfd pfd = {
        .fd = fd_key,
        .events = POLLIN,

        };

        while (1)
        {
            poll(&pfd, 1, -1);
            printf("Event occur \n");
            struct input_event ev = {0};
            read(fd_key, (void *)&ev, sizeof(ev));
            if (ev.type == EV_KEY && ev.code == KEY_1 && ev.value == 1)
            {

            led_blink_left = !led_blink_left;;
            printf(" led blink left: %d \n", led_blink_left);
        }

        if (ev.type == EV_KEY && ev.code == KEY_2 && ev.value == 1)
        {

            led_blink_right = !led_blink_right;
            printf(" led blink right: %d \n", led_blink_right);
        }

    }


}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}


void *control_led(void *arg)
{
    int fd_led = open(DEV_PATH, O_RDWR);
    if (fd_led < 0) {
        perror("open fail (/dev/led_device0)");
        return NULL;
    }

    printf("[LED] Opened: %s\n", DEV_PATH);


    bool left_on  = false;
    bool right_on = false;

    uint64_t last_left_ms  = now_ms();
    uint64_t last_right_ms = now_ms();


    QMetaObject::invokeMethod(MainWindow::instance,
                              "setLabel2Visible",
                              Qt::QueuedConnection,
                              Q_ARG(bool, false));
    QMetaObject::invokeMethod(MainWindow::instance,
                              "setLabel3Visible",
                              Qt::QueuedConnection,
                              Q_ARG(bool, false));

    while (1)
    {
        uint64_t t = now_ms();

        // ==== LEFT ====
        if (!led_blink_left) {
            if (t - last_left_ms >= 1000) {
                last_left_ms = t;
                left_on = !left_on;

                QMetaObject::invokeMethod(MainWindow::instance,
                                          "setLabel2Visible",
                                          Qt::QueuedConnection,
                                          Q_ARG(bool, left_on));

                do_ioctl_simple(fd_led, left_on ? LED1_IOC_ON : LED1_IOC_OFF);
                printf("[LED1] %s\n", left_on ? "ON" : "OFF");
            }
        } else {

            if (left_on) {
                left_on = false;
                QMetaObject::invokeMethod(MainWindow::instance,
                                          "setLabel2Visible",
                                          Qt::QueuedConnection,
                                          Q_ARG(bool, false));
                do_ioctl_simple(fd_led, LED1_IOC_OFF);
            }
            last_left_ms = t;
        }

        // ==== RIGHT ====
        if (!led_blink_right) {
            if (t - last_right_ms >= 1000) {
                last_right_ms = t;
                right_on = !right_on;

                QMetaObject::invokeMethod(MainWindow::instance,
                                          "setLabel3Visible",
                                          Qt::QueuedConnection,
                                          Q_ARG(bool, right_on));

                do_ioctl_simple(fd_led, right_on ? LED2_IOC_ON : LED2_IOC_OFF);
                printf("[LED2] %s\n", right_on ? "ON" : "OFF");
            }
        } else {
            if (right_on) {
                right_on = false;
                QMetaObject::invokeMethod(MainWindow::instance,
                                          "setLabel3Visible",
                                          Qt::QueuedConnection,
                                          Q_ARG(bool, false));
                do_ioctl_simple(fd_led, LED2_IOC_OFF);
            }
            last_right_ms = t;
        }


        usleep(100 * 1000); // 100ms
    }

     close(fd_led);
     return NULL;
}

void *get_can_msg(void *arg)
{
    QMetaObject::invokeMethod(MainWindow::instance,
                              "showSpeed",
                              Qt::QueuedConnection,
                              Q_ARG(int, 0));
    const char *ifname = "can0";

    int s;
    struct ifreq ifr;
    struct sockaddr_can addr;

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket");
        return NULL;
    }

    /* 2) Lấy chỉ số interface (ifindex) từ tên, ví dụ "main_dcan1" */
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(s);
        return NULL;
    }

    /* 3) Bind socket vào interface đó */
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(s);
        return NULL;
    }

    /* 4) (Tùy chọn) Thiết lập filter: chỉ nhận frame ID 0x123 */
    struct can_filter rfilter[1];
    rfilter[0].can_id   = 0x123;
    rfilter[0].can_mask = CAN_SFF_MASK;       // 11-bit ID
    if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER,
                   &rfilter, sizeof(rfilter)) < 0) {
        perror("setsockopt filter");
        close(s);
        return NULL;
    }

    printf("Listening on %s for CAN frames (ID 0x123)...\n", ifname);
    //data_t* can_data = NULL;

    /* 5) Vòng lặp nhận frame */
    while (1) {

        struct can_frame frame;
        ssize_t nbytes = read(s, &frame, sizeof(frame));

        if (nbytes < 0) {
            perror("read");
            break;
        } else if (nbytes < (ssize_t)sizeof(struct can_frame)) {
            fprintf(stderr, "read: incomplete CAN frame (%zd bytes)\n", nbytes);
            continue;
        }

        printf("ID=0x%03X DLC=%d Data=", frame.can_id & CAN_SFF_MASK, frame.can_dlc);

        for (int i = 0; i < frame.can_dlc; i++) {
            printf(" %02X", frame.data[i]);
        }
        printf("\n");
        if (frame.can_dlc < 2) {
            continue;
        }

        uint16_t read_adc = ((uint16_t)frame.data[1] << 8) | (uint16_t)frame.data[0];
        printf("adc : %u\n",read_adc) ;

        QMetaObject::invokeMethod(MainWindow::instance,
                                  "showSpeed",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, read_adc));

    }

    close(s);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, control_led, NULL);
    pthread_create(&t2, NULL, read_button, NULL);
    pthread_create(&t3, NULL, get_can_msg, NULL);
    return a.exec();
}



