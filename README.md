# fakesteak 

![fakesteak](example2.png)

My personal implementation of a digital character rain screen as seen in "The Matrix". 

## Overview 

The focus of this project is on low CPU and memory footprint, with the secondary 
objective being to recreate the original effect, as seen in the movie, as closely 
as possible. For simplicities sake, however, there will be no Japanese characters. 

`fakesteak` has no external dependencies (it works without ncurses), but instead 
uses some non portable code instead. There are a couple of command line options 
available, and by tweaking some of the `#defines` at the top of the file, further 
customization is quite easily possible.

I've written this on and for Linux. Everything seems to be working well using 
urxvt, xterm, lxterm or uxterm. Other OS and terminals have not yet been tested. 
Your feedback is welcome, but I don't plan on adding support for Windows/Mac OS. 

## Dependencies / Requirements

- Terminal that supports 256 colors ([8 bit color mode](https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit))
- Requires `TIOCGWINSZ` to be supported (to query the terminal size)

## Building / Running

You can just run the included `build` script. After that, you should be able to run it from the `bin` directory:

    chmod +x ./build
    ./build
    chmod +x ./bin/fakesteak
    ./bin/fakesteak

## Usage

    fakesteak [OPTIONS...]

Options:

  - `-b`: use black background color
  - `-d`: drops ratio ([1..100], default is 1)
  - `-e`: error ratio ([1..100], default is 2)
  - `-h`: print help text and exit
  - `-r`: seed for the random number generator
  - `-s`: speed factor ([1..100], default is 10)
  - `-V`: print version information and exit

The drops ratio determines the number of rain drops, as a percentage of the entire screen, 
while the error ratio influences the number of characters that will randomly change into other characters. 

## Performance

I've compared CPU and RAM usage against that of [`cmatrix`](https://github.com/abishekvashok/cmatrix), [`tmatrix`](https://github.com/M4444/TMatrix) and [`unimatrix`](https://github.com/will8211/unimatrix) a little, all of which offer better portability and more featuers. CPU usage is from `top`, memory via `smem`, looking at PSS. I've ran all programs in urxvt, with settings that give somewhat similar visual results, in a full screen terminal (1920x1080 px). Here are the approximate findings:

|                      | CPU   | RAM    | disk | Language | ran as                     |
|----------------------|-------|--------|------|----------|----------------------------|
| fakesteak v0.2.0     |  ~6 % | ~170 K | 19 K | C        | fakesteak -d33             |
|   cmatrix v2.0       |  ~7 % | ~900 K | 22 K | C        | cmatrix -b -u10            |
|   tmatrix v1.3       |  ~8 % | ~2.1 M | 87 K | C++      | tmatrix --gap=30,70        |
| unimatrix 2018/01/09 | ~11 % | ~9.4 M | 26 K | Python   | unimatrix -b -s=90 -l=o -f |

Again, when comparing these numbers, note `fakesteak`s shortcomings in features and portability compared to the other implementations.

