#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define PRESSED "pressed"
#define DEPRESSED "depressed"
#define UNKNOWN "unknown"
#define A "A"
#define B "B"
#define X "X"
#define Y "Y"
#define TOPLEFT "Top Left"
#define TOPRIGHT "Top Right"
#define BOTTOMLEFT "Bottom Left"
#define BOTTOMRIGHT "Bottom Right"
#define START "START"
#define MAYFLASH 1
#define WIIU 2
#define THRESHOLD 250

typedef struct _button_press
{
    int status;
    int button;
    int time;
} button_press;

bool check_buttons(button_press*, int);
bool compare_beats(button_press*, button_press*, int);
void record_beats(button_press*, int, int);
char* get_button(int, int);
char* get_status(int);

int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        printf("Error tcsetattr: %s\n", strerror(errno));
}

void record_beats(button_press* presses, int adapter, int password_length){
    int beats_entered = 1;

    char *portname = "/dev/input/js0";
    int fd;
    int wlen;

    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        return;
    }
    /*baudrate 115200, 8 bits, no parity, 1 stop bit */
    set_interface_attribs(fd, B115200);
    //set_mincount(fd, 0);                /* set to pure timed read */

    /* simple output */
    wlen = write(fd, "Hello!\n", 7);
    if (wlen != 7) {
        printf("Error from write: %d, %d\n", wlen, errno);
    }
    tcdrain(fd);    /* delay for output */

    int first_beat_pressed = 0;
    button_press first_beat;
    do {
        unsigned char buf[80];
        int rdlen;

        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            /* display hex */
            if (rdlen == 8 && buf[4] == 1){
                int time = buf[2] * 256 * 256 + buf[1] * 256 + buf[0];
                button_press press = { buf[4], buf[7], time };
                if (check_buttons(&press, adapter)){
                    first_beat = press;
                    first_beat_pressed = 1;
                }
            }
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        } 
        /* repeat read to get full message */
    } while (first_beat_pressed == 0);
    printf("First beat heard\n");

    //button_press presses[password_length];
    presses[0] = first_beat;
    /* simple noncanonical input */
    while (beats_entered < password_length)
    {
        unsigned char buf[80];
        int rdlen;

        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            /* display hex */
            //printf("Read %d:", rdlen);

            if (rdlen == 8 && buf[4] == 1){ 
                int time = buf[2] * 256 * 256 + buf[1] * 256 + buf[0];
                button_press press = { buf[4], buf[7], time - presses[0].time };
                if (check_buttons(&press, adapter)){
                    presses[beats_entered] = press;   
                    beats_entered++;
                }
            } /*else {
                unsigned char   *p;
                for (p = buf; rdlen-- > 0; p++)
                    printf("%d\t", *p);
                printf("\n");
            }*/
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        } 
        /* repeat read to get full message */
    }

    presses[0].time = 0;

    printf("\nYou entered this password, buster: \n");
    for(int i = 0; i < password_length; i++)
    {
        check_buttons(&presses[i], adapter);
    }
    //return presses;
}


bool compare_beats(button_press* old, button_press* new, int length){
    for (int i = 0; i < length; i++){
        if (old[i].button != new[i].button || abs(old[i].time - new[i].time) > THRESHOLD)
            return false;
    }
    return true;
}

int main()
{
    int adapter = -1;
    while (adapter != MAYFLASH && adapter != WIIU)
    {
        printf("Choose adapter: \n1 - Mayflash\n2 - Official\n");
        scanf("%d", &adapter);
    }

    int password_length = -1;
    while (password_length < 1)
    {
        printf("Choose length of password: ");
        scanf("%d", &password_length);
        printf("\n");
    }

    button_press presses[password_length];
    record_beats(presses, adapter, password_length);

    button_press new_presses[password_length];
    record_beats(new_presses, adapter, password_length);

    if (compare_beats(presses, new_presses, password_length)){
        printf("You dun do did it!\n");
    } else {
        printf("You did not dun do it!\n");
    }

    return 0;
}

bool check_buttons(button_press *press, int adapter){
    char message[80];
    memset(message, '\0', 80);
    char *str_status = get_status(press->status);
    char *str_button = get_button(press->button, adapter);
    if (strcmp(str_status, UNKNOWN) != 0 && strcmp(str_button, UNKNOWN)){
        printf("Button %s %s at time %d\n", str_button, str_status, press->time);
        return true;
    }
    return false;
}

char* get_button(int button, int adapter){
    if (adapter == MAYFLASH)
    {
        switch(button){
            case 0: return TOPRIGHT;
            case 1: return BOTTOMRIGHT;
            case 2: return BOTTOMLEFT;
            case 3: return TOPRIGHT;
            case 9: return START;
            default: return UNKNOWN;
        }
    }
    else if (adapter == WIIU)
    {
        switch(button){
            case 0: return BOTTOMRIGHT;
            case 1: return TOPRIGHT;
            case 2: return TOPLEFT;
            case 3: return BOTTOMLEFT;
            case 7: return START;
            default: return UNKNOWN;
        }
    }
}

char* get_status(int button){
    switch(button){
        case 0: return DEPRESSED;
        case 1: return PRESSED;
        default: return UNKNOWN;
    }
}