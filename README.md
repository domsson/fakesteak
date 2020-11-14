# fakesteak 

![fakesteak](example2.png)

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
    chmod +x ./bin/fakesteak
    ./bin/fakesteak

## Performance

I've compared CPU and RAM usage against that of [`cmatrix`](https://github.com/abishekvashok/cmatrix), [`tmatrix`](https://github.com/M4444/TMatrix) and [`unimatrix`](https://github.com/will8211/unimatrix) a little, all of which offer better portability and more featuers. CPU usage is from `top`, memory via `smem`, looking at PSS. I've ran all programs in urxvt, with settings that give somewhat similar visual results, in a full screen terminal (1920x1080 px). Here are the approximate findings:

|                      | CPU   | RAM    | disk | Language | ran as                     |
|----------------------|-------|--------|------|----------|----------------------------|
| fakesteak v0.2.0     |  ~6 % | ~170 K | 19 K | C        | fakesteak -d33             |
|   cmatrix v2.0       |  ~7 % | ~900 K | 22 K | C        | cmatrix -b -u10            |
|   tmatrix v1.3       |  ~8 % | ~2.1 M | 87 K | C++      | tmatrix --gap=30,70        |
| unimatrix 2018/01/09 | ~11 % | ~9.4 M | 26 K | Python   | unimatrix -b -s=90 -l=o -f |

Again, when comparing these numbers, note `fakesteak`s shortcomings in features and portability compared to the other implementations.

