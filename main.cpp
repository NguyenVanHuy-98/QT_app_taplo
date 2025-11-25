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


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

MainWindow* MainWindow::instance = nullptr;
int led_blink_left = 1;
int led_blink_right = 1;
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
        printf("ev.code: %d, ev.value: %d \n", ev.code, ev.value);
        if (ev.type == EV_KEY && ev.code == KEY_1 && ev.value == 1)
        {
            led_blink_left = !led_blink_left;
            printf("led blink left: %d \n", led_blink_left);
        }
        else if (ev.type == EV_KEY && ev.code == KEY_2 && ev.value == 1)
        {
            led_blink_right = !led_blink_right;
            printf("led blink right: %d \n", led_blink_right);
        }
    }
}

void *control_led(void *arg)
{
    int fd_led_left  = open("/sys/class/hello-kernel/device0/left", O_RDWR);
    int fd_led_right = open("/sys/class/hello-kernel/device0/right", O_RDWR);
    if (fd_led_left < 0 || fd_led_right < 0)
    {
        printf("Open fd_led_left failed (error code: %d)\n", fd_led_left);
        printf("Open fd_led_right failed (error code: %d)\n", fd_led_right);
        return NULL;
    }

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

        while (!led_blink_left)
        {
            QMetaObject::invokeMethod(MainWindow::instance,
                                      "setLabel2Visible",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, true));
            write(fd_led_left, (void *)"on", 3);
            sleep(1);
            QMetaObject::invokeMethod(MainWindow::instance,
                                      "setLabel2Visible",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, false));
            write(fd_led_left, (void *)"off", 4);
            sleep(1);
        }

        while (!led_blink_right)
        {
            QMetaObject::invokeMethod(MainWindow::instance,
                                      "setLabel3Visible",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, true));
            write(fd_led_right, (void *)"on", 3);
            sleep(1);
            QMetaObject::invokeMethod(MainWindow::instance,
                                      "setLabel3Visible",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, false));
            write(fd_led_right, (void *)"off", 4);
            sleep(1);
        }
        sleep(1);

    }
}


typedef struct {
    int package_id;
    int val;
} data_t;

void *get_can_msg(void *arg)
{
    QMetaObject::invokeMethod(MainWindow::instance,
                              "showSpeed",
                              Qt::QueuedConnection,
                              Q_ARG(int, 0));
    const char *ifname = "main_dcan1";  // đổi thành "can1" nếu bạn rename

    int s;
    struct ifreq ifr;
    struct sockaddr_can addr;

    /* 1) Tạo socket CAN RAW */
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket");
    }

    /* 2) Lấy chỉ số interface (ifindex) từ tên, ví dụ "main_dcan1" */
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(s);
    }

    /* 3) Bind socket vào interface đó */
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(s);
    }

    /* 4) (Tùy chọn) Thiết lập filter: chỉ nhận frame ID 0x123 */
    struct can_filter rfilter[1];
    rfilter[0].can_id   = 0x123;
    rfilter[0].can_mask = CAN_SFF_MASK;       // 11-bit ID
    if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER,
                   &rfilter, sizeof(rfilter)) < 0) {
        perror("setsockopt filter");
        close(s);
    }

    printf("Listening on %s for CAN frames (ID 0x123)...\n", ifname);
    data_t* can_data = NULL;

    /* 5) Vòng lặp nhận frame */
    while (1) {
        struct can_frame frame;
        can_data = (data_t*)frame.data;
        ssize_t nbytes = read(s, &frame, sizeof(frame));

        if (nbytes < 0) {
            perror("read");
            break;
        } else if (nbytes < (ssize_t)sizeof(struct can_frame)) {
            fprintf(stderr, "read: incomplete CAN frame\n");
            continue;
        }
        int speed = (can_data->val * 250)/4095;
        printf("ID=0x%03X | index = %d, data = %d, speed = %d\n", frame.can_id & CAN_SFF_MASK, can_data->package_id, can_data->val, speed);
        QMetaObject::invokeMethod(MainWindow::instance,
                                  "showSpeed",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, speed));
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



