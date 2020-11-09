# charrain

This is my personal attempt at implementing a green character rain screen as seen in the "The Matrix". There is already `cmatrix`, but I wanted to see if I could get something that looks a bit nicer and possibly runs a bit faster. In turn, I sacrifice portability and terminal compatibility.

Currently, this works pretty nicely in (u)rvxt, but flickers in other terminals I've testet it in. It assumes a terminal that supports 256 color mode.

I know all this would be easier if I just used ncurses, but part of the challenge is to see if I can do it without. The whole buffering / screen clearing flickering is pretty tough, however. If you have any advice for me, feel free to open an issue to let me know about it!
