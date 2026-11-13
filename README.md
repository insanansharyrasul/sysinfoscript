# sysinfoscript


This is just a repo contained a reinvented wheel of system information usage.
 
# But why?

My `eww` bar need to fetch a ram usage, but using "free | awk" use high ram and CPU usage for a 1s interval, so I made this. Other script maybe added in the future just for fun.

# What's the different?

Well, by writing C code, I can control the memory usage and CPU usage.

Let's fetch an example:

```c
char buf[2048];
```

I can limit the memory usage to just 2048 byte to see what's inside the `/proc/meminfo` (which is enough to read the entire file). I checked with `cat /proc/meminfo | wc -c` which output `1531`. 

You might wonder, "Why don't you just allocate with 1531 byte memory then?". On a 64-bit system, memory allocation stack mostly aligned into a 16-byte or 32-byte boundary. It's also safer to allocate 2048 byte memory incase `/proc/meminfo` grow slightly bigger.

```c
if (lseek(fd, 0, SEEK_SET) < 0) return 0;
ssize_t n = read(fd, buf, sizeof(buf) - 1);
```

```c
int fd = open("/proc/meminfo", O_RDONLY);
```

And this is the most crucial part of the program. Instead of using `fopen()` to read, I used `open()` to read the information inside `/proc/meminfo`. Using the latter directly call a direct POSIX syscall. But because of that too, the script won't work on another OS, such as Windows.

There's significant amount of CPU usage if I tried to read and close using `fopen`. On our first approach, Gemini (yes I used AI to make the script lmao), they recommended us to use `fopen` to read it.

But I know that will consume high amount of CPU, becuase I need to open (using `fopen`), reads (using `fgets`) and close the file (using `fclose`), which isn't CPU friendly to open, read, close inside a loop.

So, I open the file outside the loop and allocate the buffer outside the loop, too. After that, I can start reading the content by using `lseek()`. I also used `strstr()` to found the word "MemTotal:" and "MemAvailable:" inside the `buf` and then parse it using `sscanf` to read the content. 

I also used pointer, to change the variable `mem_total` and `mem_available`

Thus, making our RAM and CPU usage significantly lower than calling an external CLI for my `eww` bar

# What about the other script?

There's three script so far, `ram_usage`, `ram_usage_o`, and `cpu_usage`. I'm not explaining the detail on the `cpu_usage` because the different is just it fetch `/proc/stat`. `ram_usage_o` in the other side, print a ram usage, but just once (not like `ram_usage` that run it on an infinite loop). 

# How do i use it on my eww bar?

First, you can run this to install all the script to ~/.local/bin

```bash
make install
```

Or if you just need one script

```bash
make install-one TARGET=cpu_usage
```

Then, on eww bar you can write this
```
(deflisten memory        :initial "0" "ram_usage")
```

`ram_usage` accepts an optional output mode:

```bash
ram_usage       # mode 3: loop with newline output (default)
ram_usage 1     # print one value and exit
ram_usage 2     # loop with carriage-return output
ram_usage 3     # loop with newline output
```

`cpu_usage` accepts an optional output mode:

```bash
cpu_usage       # mode 3: loop with newline output (default)
cpu_usage 1     # print one value and exit
cpu_usage 2     # loop with carriage-return output
cpu_usage 3     # loop with newline output
```

The second argument sets the sampling interval in milliseconds (default: `100`):

```bash
cpu_usage 3 250 # newline output every 250 ms
```

For my example, i used it like this:
```
(defwidget memory []
    (box  :orientation "v" :space-evenly false :class "mod" :halign "center"
        (circular-progress :value memory :class "mem-ring" :thickness 4 :clockwise true
        :tooltip "RAM ${memory}%"
        (label :class "mem-icon" :text "ㅤ" :limit-width 2 :show_truncated false :wrap false)
        )
        (label :class "label-sm" :text "${memory}%")
    )
)
```

## Time delta

`dlt` calculates the elapsed time between two values. Use a quoted expression
or pass the two values separately:

```bash
dlt "12:30:30 - 12:30:45"       # 15 seconds
dlt 13/11/26 12/11/26            # 1 day
dlt "13/11/26 08:00 - 12/11/26 20:30"  # 1 day 11 hours 30 minutes
```

Accepted values are `HH:MM[:SS]` or `HH.MM[.SS]`, `DD/MM/YY`, and date-times
such as `DD/MM/YY HH:MM[:SS]` or `DD/MM/YYTHH:MM[:SS]`. Two-digit years use
the range 69-99 for 1969-1999 and 00-68 for 2000-2068.

The default output is a smart breakdown. Choose a primary unit with
`--unit day|hour|minute|second`; remaining time is shown in smaller units.
Use `--decimal` to print one decimal value, and `--signed` to preserve the
subtraction direction:

```bash
dlt --unit hour --decimal "12:30:30 - 15:00:00"  # 2.491667 hours
dlt --unit minute "12:30:30 - 15:00:00"          # 149 minutes 30 seconds
dlt --signed "12:30:30 - 15:00:00"               # -2 hours 29 minutes 30 seconds
```
