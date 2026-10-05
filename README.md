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
