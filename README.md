# Running DOOM on a ShoreTel IP485G

<video src="https://github.com/user-attachments/assets/e48a0146-2863-45df-8924-f09a64002652" controls="controls" style="max-width: 100%;"></video>


The **ShoreTel IP485G** is an office desk phone with a color LCD, speaker, keypad, ARM process with Linux, the works. I wanted to see how much of that hardware I could use for something other than its intended purpose. As per usual, my go-to is DOOM and persistent root. 

I got [**fbDOOM**](https://github.com/maximevince/fbDOOM) running directly on the phone, using its own processor, LCD, and speaker, with a USB gamepad (hid) for controls. Besides a sound driver-helper, no extra drivers or modules were needed, even the USB-HID.ko. 

Sound port took more effort than expected, good ol' ChatGPT made short work of it.

<img width="937" height="704" alt="image" src="https://github.com/user-attachments/assets/ccee1164-8055-4aaa-9cbd-25ecd840cb9d" />


## Rundown

| Feature | Status |
| --- | --- |
| **DOOM** | fbDoom running on the phone's ARM processor |
| **Display** | Framebuffer output scaled to the 480x272 LCD |
| **USB gamepad** | Working buttons and D-pad through Linux evdev /dev/input/event1 |
| **Phone buttons** | Start DOOM hotkey and working volume controls |
| **Sound effects** | Playing through the built-in speaker using `sndserver` and Broadcom /dev/halaudio |
| **LEDS** | Pick ups, damage, and shooting flashes various LEDs |
| **Music** | Plays through headset, channel 0, precompiled looped PCM |

## Hardware

| Component | Details |
| --- | --- |
| Phone | ShoreTel IP485G |
| SoC | Broadcom `BCM11107KFBG` |
| CPU | ARM1176, ARMv6 |
| Kernel | Linux 2.6.27.18 |
| Firmware C library | glibc 2.8 |
| LCD framebuffer | 480x272, 32 bit |
<br/>
<img width="771" height="520" alt="image" src="https://github.com/user-attachments/assets/34f7b0f7-f66d-493a-912b-d99f23f7dcc2" />
<br/>
<br/>

Fortunately the bring-up scripts and shipped Broadcom drivers did all of the heavy lifting and makes for a versatile computer when you strip the SIP/Phone applications.

Some of the first places to look were:

```sh
uname -a
cat /proc/cpuinfo
cat /proc/modules
cat /proc/bus/input/devices
cat /proc/halaudio
```

The normal phone application is `/voip/p8cg_phone -qws`, using qt embedded. It uses the display, input devices, audio hardware, and watchdog. Taking over those devices also meant taking over some of the work that application was doing in the background.

## PWNing the phone

Mitel sells the hardware and software, they had an incentive to make this very difficult to crack. My assumption going in was wrong. After probing some test pads with a logic analyzer, I found what is very clearly 115200 baud UART. I connected some jumpers to a TTY adapter and was immediately dropped into a root /bin/sh. No security was present. The network manager already reaches out for DHCP and dropbear was present.

<img width="316" height="435" alt="image" src="https://github.com/user-attachments/assets/46771c0f-2611-4f65-b6e9-1352db5ee931" />
<br/>
<img width="289" height="136" alt="image" src="https://github.com/user-attachments/assets/5efdaa08-fabc-46c8-9743-fa0422871ebb" />


## Inviting a stranger, fbDoom

I edited and cross-compiled on Arch Linux, copied the binaries to the phone via SCP, then tested over SSH.

The phone runs an old enough Linux environment that “compile it for ARM” was not specific enough. Early builds failed during runtime startup around `__init_ssp` and thread local storage, before reaching the game. Another attempted build failed with an illegal instruction.

The working compiler settings used Zig, static linking, soft-float, and software thread pointer access:

```sh
make clean
make NOSDL=1 \
  CC="zig cc -target arm-linux-musleabi -mcpu=arm1136jf_s+soft_float -mtp=soft -marm -static" \
  CFLAGS="-O2 -Wall -DNORMALUNIX -DLINUX"
```

| Option | Why it was used |
| --- | --- |
| `NOSDL=1` | Use the fbDoom fork's non-SDL path |
| `-target arm-linux-musleabi` | Build for 32 bit ARM Linux with musl |
| `-mcpu=arm1136jf_s+soft_float` | No issues, may be more specific targets out there |
| `-mtp=soft` | Software thread pointer access (rather than thread-pointer CPU register) |
| `-marm` | Don't use thumb mode |
| `-static` | Build as a single clean binary |

### Talking to the phone

Remove the default dropbear instance and start a new one without default cert-auth flags

```sh
killall dropbear & /usr/sbin/dropbear -p 22
```

From the computer, the SSH server needed compatibility options:

```sh
ssh -p 22 \
  -o HostKeyAlgorithms=+ssh-rsa \
  -o PubkeyAuthentication=no \
  -o PreferredAuthentications=password \
  root@phone_ip
```

UART provided boot output at 115200 baud with `ttyAMA0` as the kernel console. SSH is more convenient and required for file transfers 

## Woof woof

Stopping `p8cg_phone` frees up the hardware for DOOM (by about 80mb and CPU util). It also removes the application that normally keeps the hardware watchdog alive. In my testing the phone would reset after roughly 30 seconds without it.

The watchdog is accessible through `/dev/watchdog`. I used a background shell loop to open it once as file descriptor 3, then write to it every five seconds to keep the phone from resetting:

```sh
(
    exec 3>/dev/watchdog || exit 1
    while true; do
        printf x >&3
        sleep 5
    done
) &
```

## Writing pixels to the framebuffer

The display appears at `/dev/fb0`. DOOM renders a 320x200 buffer palette indexes, while the phone expects 480x272 pixels at 32 bits per pixel.

For each destination pixel, the framebuffer patch finds the corresponding source pixel:

```c
sx = x * SCREENWIDTH  / fb.xres;
sy = y * SCREENHEIGHT / fb.yres;
```

It looks up the pixel's color in DOOM's palette and converts it to the phone's 32 bit display format.

This is nearest-neighbor scaling. It is simple, uses integer arithmetic, and fills the phone's display. It also stretches the image to the LCD's proportions.

(before and after scaling)
<img width="958" height="249" alt="image" src="https://github.com/user-attachments/assets/8b9db7c1-0874-4a5d-b196-cffad6459760" />

## Adding a USB gamepad

The phone's keypad appears at `/dev/input/event0`, and I experimented with translating its keys into DOOM controls. Playing it with the keypad wasn't very fun so I wanted to see how I could utilize the USB. I was very fortunate to see a USB HID driver to facilitate this.

USB HID support had to be loaded manually:

```sh
/rootfs/sbin/insmod \
  /lib/modules/2.6.27.18/kernel/drivers/hid/usbhid/usbhid.ko
```

After that, `/proc/bus/input/devices` listed the controller with `Handlers=event1`, but there was still no device node. For this setup I created it with:

```sh
mknod /dev/input/event1 c 13 65
```

This only exposes `/dev/input/event1` the kernel has already registered.

The input backend reads `struct input_event` records, translates them into DOOM key events, and passes them to `D_PostEvent()` (i_input_tty.c)

| Control | Event code | Action |
| --- | --- | --- |
| D-pad | Axes 3 and 4 | Move and turn |
| X | `0x120` | Run |
| A | `0x121` | Use/Open |
| B | `0x122` | Fire |
| Y | `0x123` | Map |
| LT | `0x124` | Strafe |
| RT | `0x125` | Next weapon |
| LT + RT | `0x00` | Previous weapon |
| Select | `0x128` | Escape/Menu |
| Start | `0x129` | Enter |
| Phone Transfer| `0x2f` | Launch DOOM |
| Phone volume down | `0x2d` | Decrease volume |
| Phone volume up | `0x2b` | Increase volume |

<img width="747" height="219" alt="image" src="https://github.com/user-attachments/assets/99ddb5ea-3593-4dac-80c6-b7ad0ce0f80f" />



### The message light has a new job

Holding the fire button writes `100` to:

```text
/sys/class/leds/message:red/**brightness**
```

Releasing it writes `0`. It can light even if the game cannot actually fire a shot, good enough for a one-week port.

Debug executable `ftled` echos which leds correspond to which button/indicator. This was used to select which to light up during damage and pickups

<img width="919" height="346" alt="image" src="https://github.com/user-attachments/assets/10263be2-cef0-4de4-b944-3a03e6e091f7" />



## Making the speaker work

The useful audio pipe was Broadcom's HALAUDIO interface. Relevant devices included `/dev/halaudio`, `/dev/amxr`, and `/dev/ept0`, with vendor modules already loaded by the firmware.

Before connecting any of this to DOOM, I used a small beep program to find a working output configuration. Codec/channel 1 drove the physical speaker.

| Setting | Working value |
| --- | --- |
| Output device | `/dev/halaudio` |
| Sample rate | 16,000 hz |
| Channels | Mono |
| Format | Signed 16 bit LE PCM |
| Period | 5 ms |
| Samples per period | 80 |
| Bytes per period | 160 |

The backend uses `ioctl()` calls for power, rate, and format, then `write()` for the actual PCM samples.

<img width="654" height="396" alt="image" src="https://github.com/user-attachments/assets/682f1e68-b15f-4664-847f-d055dc39d1af" />


### Connecting DOOM to sndserver

I kept the external sound server from the fbDoom fork and added a wrapper, `i_shoretel_sndserv.c`, to send it sound effect requests. The wrapper starts the server through `popen()`:

```sh
DOOMWADDIR=/data/fbdoom /data/fbdoom/sndserver -quiet
```

An example play command is:

```text
p01807f80
```

The fields are `p` for play, effect `01`, pitch `80`, volume `7f`, and stereo position `80`. The pipe sends commands then sndserver loads and mixes the sound data itself.

The sound server outputs 11.025 khz stereo. The phone backend combines the left and right channels into mono, then resamples it to 16 kHz. halaudio receives the result in 5 ms blocks: 80 samples / 160 bytes per block.

<img width="1194" height="283" alt="image" src="https://github.com/user-attachments/assets/e9251348-e766-43f3-ac75-f21a88c8071f" />

### Possible recovery paths

The phone application startup script `/etc/rc.d/S100applic` starts the UART shell, if this script fails and no other backup or earlier shells are established, the phone is **SOFT BRICKED**. There is absolutely no *(easy)* means of recovery. The NAND flash TC58NVG1S3HTA00 chip can be extracted for recovery with suitable tools (those costing several times more than this phone on ebay). I do not have those tools.

<img width="389" height="470" alt="image" src="https://github.com/user-attachments/assets/f0d8a866-54b2-407d-84a8-5b6b1a8397bb" />


Happy hacking!
