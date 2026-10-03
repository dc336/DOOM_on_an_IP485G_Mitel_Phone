/*
 * i_input_tty.c
 *
 * ShoreTel / BCMRING USB gamepad input backend for fbDOOM.
 *
 * Device:
 *     /dev/input/event1
 *
 * Controller mapping discovered from evdev:
 *
 *   X      = 0x120  -> run
 *   A      = 0x121  -> use / open
 *   B      = 0x122  -> fire
 *   Y      = 0x123  -> automap
 *   LT     = 0x124  -> strafe modifier
 *   RT     = 0x125  -> next weapon
 *   LT+RT            -> previous weapon
 *   SELECT = 0x128  -> Escape / menu
 *   START  = 0x129  -> Enter / confirm
 *
 * D-pad:
 *
 *   EV_ABS code 3:
 *       0   = left
 *       127 = center
 *       255 = right
 *
 *   EV_ABS code 4:
 *       0   = up
 *       127 = center
 *       255 = down
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/input.h>

#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "d_main.h"

#define MESSAGE_LED "/sys/class/leds/message:red/brightness"


static void SetMessageLED(int on)
{
    int fd;

    fd = open(MESSAGE_LED, O_WRONLY);

    if (fd < 0)
    {
        return;
    }

    if (on)
    {
        write(fd, "100\n", 4);
    }
    else
    {
        write(fd, "0\n", 2);
    }

    close(fd);
}

/*
 * Some fbDOOM code expects this symbol to exist.
 */
int vanilla_keyboard_mapping = 1;


/*
 * Gamepad fd.
 */
static int input_fd = -1;


/*
 * D-pad state.
 *
 * We remember these so that changing an axis from:
 *
 *     LEFT -> CENTER
 *
 * generates a proper KEY_LEFTARROW release.
 */
static int pad_up    = 0;
static int pad_down  = 0;
static int pad_left  = 0;
static int pad_right = 0;

/* Left-trigger state is also used as a weapon-cycle modifier. */
static int lt_down = 0;

/*
 * Weapon slot used by the controller's cycle buttons.
 * Vanilla Doom selects weapons with number keys 1-7.
 */
static int weapon_cycle_slot = 2;


/*
 * Send a Doom keyboard event.
 */
static void PostKey(int down, int key)
{
    event_t event;

    event.type  = down ? ev_keydown : ev_keyup;
    event.data1 = key;
    event.data2 = 0;
    event.data3 = 0;

    D_PostEvent(&event);
}


/*
 * Change a virtual Doom key only if its state actually changed.
 */
static void UpdateKey(int *old_state, int new_state, int key)
{
    if (*old_state == new_state)
    {
        return;
    }

    *old_state = new_state;

    PostKey(new_state, key);
}


/*
 * Cycle through Doom's weapon number keys.
 *
 * direction > 0 : next weapon
 * direction < 0 : previous weapon
 *
 * Doom ignores a weapon-number key when that weapon is unavailable, so
 * repeated presses naturally move through the available arsenal.
 */
static void CycleWeapon(int direction)
{
    int key;

    if (direction > 0)
    {
        weapon_cycle_slot++;

        if (weapon_cycle_slot > 7)
            weapon_cycle_slot = 1;
    }
    else
    {
        weapon_cycle_slot--;

        if (weapon_cycle_slot < 1)
            weapon_cycle_slot = 7;
    }

    key = '0' + weapon_cycle_slot;

    printf("weapon cycle: slot=%d key=%c\n",
           weapon_cycle_slot, key);

    PostKey(1, key);
    PostKey(0, key);
}


/*
 * Handle EV_KEY gamepad buttons.
 */
static void HandleButton(unsigned int code, int value)
{
    int down;

    /*
     * Linux evdev:
     *
     *   0 = released
     *   1 = pressed
     *   2 = repeat
     *
     * Doom tracks held state itself, so ignore repeats.
     */
    if (value == 2)
    {
        return;
    }

    down = (value == 1);

    switch (code)
    {
        /*
         * X = run
         */
        case 0x120:
            PostKey(down, KEY_RSHIFT);
            break;


        /*
         * A = use / open doors
         */
        case 0x121:
            PostKey(down, KEY_USE);
            break;


        /*
         * B = fire
         */
		case 0x122:
			PostKey(down, KEY_FIRE);
			SetMessageLED(down);
			break;


        /*
         * Y = automap
         */
        case 0x123:
            PostKey(down, KEY_TAB);
            break;


        /*
         * Left trigger = strafe modifier.
         *
         * Holding LT while pressing RT changes RT from next weapon to
         * previous weapon.
         */
        case 0x124:
            lt_down = down;
            PostKey(down, KEY_RALT);
            break;


        /*
         * Right trigger:
         *   RT       = next weapon
         *   LT + RT  = previous weapon
         *
         * Weapon switching is a one-shot action, so only act on press.
         */
        case 0x125:
            if (down)
            {
                CycleWeapon(lt_down ? -1 : 1);
            }
            break;


        /*
         * SELECT = Escape / menu
         */
        case 0x128:
            PostKey(down, KEY_ESCAPE);
            break;


        /*
         * START = Enter
         */
        case 0x129:
            PostKey(down, KEY_ENTER);
            break;


        default:
            break;
    }
}


/*
 * Handle the D-pad.
 */
static void HandleAxis(unsigned int code, int value)
{
    int negative;
    int positive;

    /*
     * Your gamepad normally reports:
     *
     *     0   negative direction
     *     127 center
     *     255 positive direction
     *
     * Use thresholds rather than checking exactly 0/127/255.
     */
    negative = (value < 64);
    positive = (value > 192);


    /*
     * ABS_RX / code 3:
     *
     *     0   = left
     *     127 = center
     *     255 = right
     */
    if (code == 3)
    {
        UpdateKey(&pad_left,
                  negative,
                  KEY_LEFTARROW);

        UpdateKey(&pad_right,
                  positive,
                  KEY_RIGHTARROW);
    }


    /*
     * ABS_RY / code 4:
     *
     *     0   = up
     *     127 = center
     *     255 = down
     */
    else if (code == 4)
    {
        UpdateKey(&pad_up,
                  negative,
                  KEY_UPARROW);

        UpdateKey(&pad_down,
                  positive,
                  KEY_DOWNARROW);
    }
}


/*
 * Open USB gamepad.
 */
static int kbd_init(void)
{
    input_fd = open("/dev/input/event1",
                    O_RDONLY | O_NONBLOCK);

    if (input_fd < 0)
    {
        perror("Unable to open /dev/input/event1");
        return 1;
    }

    printf("USB gamepad: /dev/input/event1\n");

    return 0;
}


/*
 * fbDOOM references this externally, so do NOT make it static.
 */
void kbd_shutdown(void)
{
    if (input_fd >= 0)
    {
        close(input_fd);
        input_fd = -1;
    }

    SetMessageLED(0);

    pad_up    = 0;
    pad_down  = 0;
    pad_left  = 0;
    pad_right = 0;
    lt_down   = 0;
}


/*
 * Called continuously by Doom.
 *
 * Consume every pending evdev event without blocking.
 */
void I_GetEvent(void)
{
    struct input_event input_event;
    ssize_t result;

    if (input_fd < 0)
    {
        return;
    }

    for (;;)
    {
        result = read(input_fd,
                      &input_event,
                      sizeof(input_event));

        if (result == sizeof(input_event))
        {
            switch (input_event.type)
            {
                case EV_KEY:
                    HandleButton(input_event.code,
                                 input_event.value);
                    break;


                case EV_ABS:
                    HandleAxis(input_event.code,
                               input_event.value);
                    break;


                /*
                 * EV_SYN just terminates a group of Linux
                 * input events. Doom doesn't need it.
                 */
                case EV_SYN:
                    break;


                /*
                 * Your controller also emits EV_MSC.
                 * We don't need those scan-code records.
                 */
                case EV_MSC:
                    break;


                default:
                    break;
            }

            continue;
        }


        /*
         * Nonblocking fd with nothing more to read.
         */
        if (result < 0
         && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }


        /*
         * Device disappeared or some real read error occurred.
         */
        if (result <= 0)
        {
            break;
        }


        /*
         * Partial input_event shouldn't normally happen.
         */
        break;
    }
}


/*
 * Called by fbDOOM during startup.
 */
void I_InitInput(void)
{
    kbd_init();
}