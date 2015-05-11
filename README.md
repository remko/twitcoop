# [TwitCoop: A Desktop Cage for Twitter Mobile Web] (http://el-tramo.be/software/twitcoop)

## About

A standalone application to access the Twitter Mobile Web interface.

**Update: The mobile Twitter website was updated, and doesn't work 
anymore with TwitCoop. I was not able to easily fix this, and no longer have
the time or need for it, so this project is therefore abandoned.  
An alternative on Mac OS X is to use FluidApp and use an iOS iPhone user agent, which has
the same effect that this application used to have.**

Features include:

- Compact
- Provides an "official", consistent Twitter interface
- Automatically refreshes when new tweets are available (with a higher refresh rate than the default mobile web interface)
- Opens external (i.e. non-Twitter) links in your preferred web browser
- Allows you to hide the Tweet box, when screen real estate is a problem (e.g. on netbooks)	
- Allows you to customize the zoom level
- Automatically logs you into your previous Twitter session on startup
- Supports "kinetic scrolling"
- Works on Windows and Linux

## Building

To build, run:

    qmake
    make (or nmake on Windows)

