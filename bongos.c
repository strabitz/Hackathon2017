#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/Xutil.h>
#include <sys/wait.h>
#include <assert.h>
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
#define BONGO_RADIUS 100
#define NIL (0)

#define MIN_LENGTH 1
#define DEFAULT_LENGTH 4
#define MAX_LENGTH 16

Display *dpy;
GC gc;
Window w;
Colormap colormap;
int adapter = WIIU;

typedef struct _bongo_state {
    bool top_left;
    bool top_right;
    bool bottom_left;
    bool bottom_right;
} bongo_state;

typedef struct _button_press
{
    int status;
    int button;
    int time;
} button_press;



void reset_state(bongo_state*);
int set_interface_attribs(int , int);
void draw_bongos(bongo_state);
void generate_window();
bool check_buttons(bongo_state*, button_press*);
bool compare_beats(button_press*, button_press*, int);
void record_beats(bongo_state*, button_press*, int, int);
char* get_button(int);
char* get_status(int);
bool bongo_decision(bongo_state, char*, char*);
int select_length(bongo_state, char*, char*);


void reset_state(bongo_state* state){
    state->bottom_left = false;
    state->bottom_right = false;
    state->top_left = false;
    state->top_right = false;
}

int select_length(bongo_state state, char* a, char* b){
    int result = DEFAULT_LENGTH;

    char *portname = "/dev/input/js0";
    int fd;
    int wlen;

    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        exit(-1);
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

    do {
        draw_bongos(state);
        char temp[8];
        memset(temp, '\0', 8);
        sprintf(temp, "%d", result);
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 1.25f, BONGO_RADIUS * 1.25f, a, strlen(a));
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 2.5f, BONGO_RADIUS * 1.25f, temp, strlen(temp));
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 3.75f, BONGO_RADIUS * 1.25f, b, strlen(b));
        XFlush(dpy);

        unsigned char buf[80];
        int rdlen;

        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            /* display hex */
            if (rdlen == 8 && buf[4] == 1){
                int time = buf[2] * 256 * 256 + buf[1] * 256 + buf[0];
                button_press press = { buf[4], buf[7], time };
                if (check_buttons(&state, &press)){
                    if (strcmp(get_button(press.button), TOPLEFT) == 0 || strcmp(get_button(press.button), BOTTOMLEFT) == 0){
                        if (result > MIN_LENGTH) result--;
                    }
                    else if (strcmp(get_button(press.button), START) == 0)
                        return result;
                    else {
                        if (result < MAX_LENGTH) result++;
                    }
                }
            }
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
        /* repeat read to get full message */

        draw_bongos(state);
        memset(temp, '\0', 8);
        sprintf(temp, "%d", result);
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 1.25f, BONGO_RADIUS * 1.25f, a, strlen(a));
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 2.5f, BONGO_RADIUS * 1.25f, temp, strlen(temp));
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 3.75f, BONGO_RADIUS * 1.25f, b, strlen(b));
        XFlush(dpy);
    } while (1);
}

bool bongo_decision(bongo_state state, char* a, char* b){
    draw_bongos(state);
    XDrawImageString(dpy, w, gc, BONGO_RADIUS * 1.25f, BONGO_RADIUS * 1.25f, a, strlen(a));
    XDrawImageString(dpy, w, gc, BONGO_RADIUS * 3.75f, BONGO_RADIUS * 1.25f, b, strlen(b));
    XFlush(dpy);

    char *portname = "/dev/input/js0";
    int fd;
    int wlen;

    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        exit(-1);
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

    do {
        draw_bongos(state);
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 1.25f, BONGO_RADIUS * 1.25f, a, strlen(a));
        XDrawImageString(dpy, w, gc, BONGO_RADIUS * 3.75f, BONGO_RADIUS * 1.25f, b, strlen(b));
        XFlush(dpy);
        unsigned char buf[80];
        int rdlen;

        rdlen = read(fd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            /* display hex */
            if (rdlen == 8 && buf[4] == 1){
                int time = buf[2] * 256 * 256 + buf[1] * 256 + buf[0];
                button_press press = { buf[4], buf[7], time };
                if (check_buttons(&state, &press)){
                    draw_bongos(state);
                    XDrawImageString(dpy, w, gc, BONGO_RADIUS * 1.25f, BONGO_RADIUS * 1.25f, a, strlen(a));
                    XDrawImageString(dpy, w, gc, BONGO_RADIUS * 3.75f, BONGO_RADIUS * 1.25f, b, strlen(b));
                    XFlush(dpy);

                    if (strcmp(get_button(press.button), TOPLEFT) == 0 || strcmp(get_button(press.button), BOTTOMLEFT) == 0)
                        return true;
                    else
                        return false;
                }
            }
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
        /* repeat read to get full message */
    } while (1);
}

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

void record_beats(bongo_state* state, button_press* presses, int adapter, int password_length){
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
                if (check_buttons(state, &press)){
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
                if (check_buttons(state, &press)){
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
        check_buttons(state, &presses[i]);
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
    bongo_state state = {0};

    generate_window();
    draw_bongos(state);
    if (bongo_decision(state, "Mayflash", "Wii U")){
        adapter = MAYFLASH;
    }
    printf("Chose %s", adapter == MAYFLASH ? "Mayflash" : "Wii U");
    draw_bongos(state);

    int password_length = select_length(state, "-", "+");
    do {
        button_press presses[password_length];
        button_press new_presses[password_length];
        if (bongo_decision(state, "Record", "Play")){
            printf("RECORDING PASSWORD\n");
            record_beats(&state, presses, adapter, password_length);
            printf("PASSWORD RECORDED\n");
        } else {
            printf("CHECKING PASSWORD\n");
            record_beats(&state, new_presses, adapter, password_length);
            if (compare_beats(presses, new_presses, password_length)){
                printf("SUCCESS!\n");
            } else {
                printf("FAILURE!\n");
            }
        }
        reset_state(&state);
        draw_bongos(state);
    } while (1);


    /*int adapter = -1;
    while (adapter != MAYFLASH && adapter != WIIU)
    {
        printf("Choose adapter: \n1 - Mayflash\n2 - Official\n");
        scanf("%d", &adapter);
    }*/

    /*int password_length = -1;
    while (password_length < 1)
    {
        printf("Choose length of password: ");
        scanf("%d", &password_length);
        printf("\n");
    }*/

    return 0;
}

bool check_buttons(bongo_state* state, button_press *press){
    char message[80];
    memset(message, '\0', 80);
    char *str_status = get_status(press->status);
    char *str_button = get_button(press->button);
    if (strcmp(str_button, UNKNOWN) != 0){
        if (strcmp(str_button, BOTTOMLEFT) == 0 && strcmp(str_status, PRESSED) == 0){
            state->bottom_left = true;
        } else if (strcmp(str_button, BOTTOMLEFT) == 0){
            state->bottom_left = false;
        }

        if (strcmp(str_button, TOPLEFT) == 0 && strcmp(str_status, PRESSED) == 0){
            state->top_left = true;
        } else if (strcmp(str_button, TOPLEFT) == 0){
            state->top_left = false;
        }

        if (strcmp(str_button, TOPRIGHT) == 0 && strcmp(str_status, PRESSED) == 0){
            state->top_right = true;
        } else if (strcmp(str_button, TOPRIGHT) == 0){
            state->top_right = false;
        }

        if (strcmp(str_button, BOTTOMRIGHT) == 0 && strcmp(str_status, PRESSED) == 0){
            state->bottom_right = true;
        } else if (strcmp(str_button, BOTTOMRIGHT) == 0){
            state->bottom_right = false;
        }

        if (strcmp(str_status, UNKNOWN) != 0){
            printf("Button %s %s at time %d\n", str_button, str_status, press->time);
            return true;
        }
    }
    return false;
}

char* get_button(int button){
    if (adapter == MAYFLASH)
    {
        switch(button){
            case 0: return TOPRIGHT;
            case 1: return BOTTOMRIGHT;
            case 2: return BOTTOMLEFT;
            case 3: return TOPLEFT;
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

void draw_bongos(bongo_state state){
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    XFillRectangle(dpy, w, gc, 0, 0, BONGO_RADIUS * 5, BONGO_RADIUS * 2.5f);
    XFlush(dpy);
    XSetForeground(dpy, gc, WhitePixel(dpy, DefaultScreen(dpy)));
    if (state.top_left){
        XFillArc(dpy, w, gc, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, 180 * 64);
    } else {
        XDrawArc(dpy, w, gc, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, 180 * 64);
    }

    if (state.bottom_left){
        XFillArc(dpy, w, gc, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, -180 * 64);
    } else {
        XDrawArc(dpy, w, gc, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, -180 * 64);
    }

    if (state.top_right){
        XFillArc(dpy, w, gc, BONGO_RADIUS * 2.75f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, 180 * 64);
    } else {
        XDrawArc(dpy, w, gc, BONGO_RADIUS * 2.75f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, 180 * 64);
    }

    if (state.bottom_right){
        XFillArc(dpy, w, gc, BONGO_RADIUS * 2.75f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, -180 * 64);
    } else {
        XDrawArc(dpy, w, gc, BONGO_RADIUS * 2.75f, BONGO_RADIUS * 0.25f, BONGO_RADIUS * 2, BONGO_RADIUS * 2, 0, -180 * 64);
    }
    XFlush(dpy);
}

void generate_window(){
    dpy = XOpenDisplay(NIL);
    assert(dpy);

    int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
    int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

    // Create the window
    w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, BONGO_RADIUS * 5, BONGO_RADIUS * 2.5f, 0, blackColor, blackColor);

    // We want to get MapNotify events
    XSelectInput(dpy, w, StructureNotifyMask);

    // "Map" the window (that is, make it appear on the screen)
    XMapWindow(dpy, w);

    // Create a "Graphics Context"
    gc = XCreateGC(dpy, w, 0, NIL);

    colormap = DefaultColormap(dpy, DefaultScreen(dpy));

    // Tell the GC we draw using the white color
    XSetForeground(dpy, gc, whiteColor);

    // Wait for the MapNotify event
    for(;;) {
	    XEvent e;
	    XNextEvent(dpy, &e);
	    if (e.type == MapNotify)
	        break;
    }
}
