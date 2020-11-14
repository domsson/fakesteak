# fakesteak 

![fakesteak](example2.png)

> You know, I know this steak doesn't exist. I know that when I put it in my mouth, the Matrix is telling my brain that it is juicy and delicious. After nine years, you know what I realize? Ignorance is bliss.

 -- _Cypher_

## Overview 

My take at an implementation of the [digital character rain](https://en.wikipedia.org/wiki/Matrix_digital_rain) as seen in "The Matrix". 

The focus of this project is on low CPU and memory footprint, with the secondary 
objective being to recreate the original effect, as seen in the movie, as closely 
as possible. For simplicities sake, however, there will be no Japanese characters. 

`fakesteak` has no external dependencies (it works without ncurses), but uses 
some non portable code instead. There are some command line options available, 
and by tweaking some of the `#defines` at the top of the file, further 
customization - for example of the colors - is easily possible.

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
  - `-d`: drops ratio ([1..100], default is 10)
  - `-e`: error ratio ([1..100], default is 2)
  - `-h`: print help text and exit
  - `-r`: seed for the random number generator
  - `-s`: speed factor ([1..100], default is 10)
  - `-V`: print version information and exit

The drops ratio determines the density of the matrix, while the error ratio influences
the number of glitches in the matrix (randomly changing characters). 

## Performance

I've compared CPU and RAM usage against that of some other popular matrix rain implementations a little. CPU usage is from `top`, memory via `smem`, looking at PSS. I've ran all programs in urxvt, with settings that give somewhat similar visual results, in a full screen terminal (1920x1080 px). Here are the approximate findings:

|                      | CPU      | RAM    | disk  | Language | ran as                      |
|----------------------|----------|--------|-------|----------|-----------------------------|
| fakesteak v0.2.0     |     ~6 % | ~170 K |  19 K | C        | fakesteak -d33              |
|   cmatrix v2.0       |     ~7 % | ~900 K |  22 K | C        | cmatrix -b -u10             |
|   tmatrix v1.3       |     ~8 % | ~2.1 M |  87 K | C++      | tmatrix --gap=30,70         |
| unimatrix 2018/01/09 |    ~11 % | ~9.4 M |  26 K | Python   | unimatrix -b -s=90 -l=o -f  |
| cxxmatrix 2020/09/27 | ~20-55 % | ~4.5 M | 124 K | C++      | cxxmatrix -s 'rain-forever' |

Note, however, that some of these other projects offer more features and/or portability.

## Support

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L22BUD8)

## Related Projects

- [cmatrix](https://github.com/abishekvashok/cmatrix)
- [tmatrix](https://github.com/M4444/TMatrix)
- [unimatrix](https://github.com/will8211/unimatrix)
- [cxxmatrix](https://github.com/akinomyoga/cxxmatrix)
