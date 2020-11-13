# charrain

![charrain](example2.png)

This is my personal implementation of a green character rain screen as seen in "The Matrix". 

## Objectives

- Use as few resources (memory, CPU) as possible
- Use as few dependencies as possible (no ncurses)
- Get as close to the original as possible, minus Japanese characters
- Assumptions over options, at least for the time being
- Compatibility and portability can be sacrificied

## State

Everything seems to be working well on Linux, using urxvt, xterm, lxterm or uxterm. Other OS and terminals have not yet been tested.

## Dependencies / Requirements

- Terminal that supports 256 colors ([8 bit color mode](https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit))
- Requires `TIOCGWINSZ` to be supported (to query the terminal size)

## Building / Running

You can just run the included `build` script. After that, you should be able to run it from the `bin` directory:

    chmod +x ./build
    ./build
    chmod +x ./bin/charrain
    ./bin/charrain

## Performance

I've compared CPU and RAM usage against that of [`cmatrix`](https://github.com/abishekvashok/cmatrix), [`tmatrix`](https://github.com/M4444/TMatrix) and [`unimatrix`](https://github.com/will8211/unimatrix) a little, all of which offer better portability and more featuers. CPU usage is from `top`, memory via `smem`, looking at PSS. I've ran all programs in urxvt, with settings that give somewhat similar visual results, in a full screen terminal (1920x1080 px). Here are the approximate findings:

|           | CPU   | RAM    | Language | ran as                     |
|-----------|-------|--------|----------|----------------------------|
|  charrain |  ~7 % | ~170 K | C        | charrain                   |
|   cmatrix |  ~7 % | ~900 K | C        | cmatrix -b -u10            |
|   tmatrix |  ~6 % | ~2.5 M | C++      | tmatrix                    |
| unimatrix | ~11 % | ~9.4 M | Python   | unimatrix -b -s=90 -l=o -f |

It looks like `tmatrix` is the most efficient regarding CPU time, but also uses the most amount of memory amongst the C/C++ implementations. `cmatrix` and `charrain` seem to perform very similar in the CPU department, but `charrain` only uses a fraction of the memory. As expected, `unimatrix` uses the most CPU time and memory.

Note, however, `charrain`s shortcomings in features and portability compared to the other three.

