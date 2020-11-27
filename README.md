# fakesteak 

[![fakesteak asciicast](https://asciinema.org/a/z9gykqvDEfmh9aKVlJtu3LOCs.svg)](https://asciinema.org/a/z9gykqvDEfmh9aKVlJtu3LOCs)

> You know, I know this steak doesn't exist. I know that when I put it in my mouth, the Matrix is telling my brain that it is juicy and delicious. After nine years, you know what I realize? Ignorance is bliss.

 -- _Cypher_

## Overview 

My take at an implementation of the [digital character rain](https://en.wikipedia.org/wiki/Matrix_digital_rain) 
as seen in "The Matrix". 

Some things you might like about fakesteak:

 - Small footprint (low on RAM and disk usage)
 - Good performance (low on CPU usage)
 - Looks pretty close to the original (fading, glitches)
 - Basic customization via command line options
 - No dependencies (not even ncurses)
 - Clean, well commented code
 - Public Domain

Some things that might rub you the wrong way:

 - No Japanese characters (for simplicity's sake)
 - Not cross-platform (no Win/Mac)

Successfully tested on Linux (urxvt, xterm, lxterm, uxterm) and FreeBSD (st). 
Feedback on compatibility with your OS / distro / terminal is very welcome; 
you can open an issue to let me know.

## Dependencies / Requirements

- Terminal that supports 256 colors ([8 bit color mode](https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit))
- Requires `TIOCGWINSZ` to be supported (to query the terminal size)
- Requires Make for building

## Building / Running

You can just run `make`. After that, you should be able to run it from the `bin` directory:

    make
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

Since the main focus of `fakesteak` is performance, I tried comparing it to other popular 
implementations. **Note, however, that this comparison is inaccurate and unfair at best**, 
mainly for the following reasons:

- All projects have different design goals, feature sets and visual fidelity
- The measurements are just rounded estimates aquired from `top` and `smem -tk` (PSS)

For example, `fakesteak` is \*nix only and does not support Japanese Katakana characters, 
while most other projects are cross-platform and do have Kana support. Also, note how `cxxmatrix`, 
for example, focuses on visuals, rendering three layers of rain with a glow effect. 
See [this reddit thread](https://www.reddit.com/r/unixporn/comments/ju62xa/oc_fakesteak_yet_another_matrix_rain_generator/gcdu5tl/) 
for further discussion.

All projects were run in a 1920 x 1080 px urxvt terminal with options that give _somewhat_ 
similar visual results. At least the speed of the rain is almost exactly the same for all, 
but density and visual fidelity (Kanas, fading, glow, etc) do differ. See the "arguments" 
column below to see how I've ran each implementation.

|                      | CPU      | RAM    | disk  | Lang.    | arguments                                          |
|----------------------|----------|--------|-------|----------|----------------------------------------------------|
| fakesteak v0.2.0     |     ~5 % | ~170 K |  19 K | C        | `-d 15`                                            |
|   cmatrix v2.0       |     ~8 % | ~900 K |  22 K | C        | `-b -u 10`                                         |
|   tmatrix v1.3       |     ~9 % | ~2.0 M |  87 K | C++      | `-g 30,70 -f 1,1 -c default`                       |
| cxxmatrix 2020-11-18 |  ~13\* % | ~4.5 M | 114 K | C++      | `-s rain-forever --frame-rate=10 --error-rate=0.1` |
| unimatrix 2018-01-09 |    ~15 % | ~9.4 M |  26 K | Python   | `-s 90 -l=o -f`                                    |

**Note**: cxxmatrix behaves a bit odd on my system. If the terminal window is visible, 
it uses about 13% CPU. If it isn't (for example, by switching to another workspace), 
the CPU load doubles.

## Support

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L22BUD8)

## Related Projects

- [cmatrix](https://github.com/abishekvashok/cmatrix)
- [tmatrix](https://github.com/M4444/TMatrix)
- [unimatrix](https://github.com/will8211/unimatrix)
- [cxxmatrix](https://github.com/akinomyoga/cxxmatrix)
