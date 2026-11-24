<div align="center">
	<img src="assets/fbi.png" style="margin: 0">
	<h1 style="margin: 0">
		fbi-dm
	</h1>
</div>

<div align="center">
	Framebuffer image/info display manager
</div>

## Why do this?
I saw this post [reddit post](https://www.reddit.com/r/Gentoo/comments/1g63itl/\
does_anyone_know_how_to_recreate_this/) that showed a neat little image and prompt
during the boot process. As a Touhou fan and someone who likes to make things look
the way I want I made this.

Previously I was using a bash script and [imgfb](https://github.com/sophuric/imgfb) 
to achieve this which worked but only somewhat. It is now being written in C with 
minimal dependencies so I can learn play around with it.

## Installation
To install simply clone, make and make install:
```
git clone https://git.ethanbtst.ca/fbi-dm && cd fbi-dm
make
sudo make install
```

After, disable any display manager on tty and enable `fbi-dm@tty1.service`

## Customization
To customize edit `/opt/fbi-dm/config.ini`

By default the images shown are the penguins found when booting the
system (if enabled when building the kernel). The big image will be
your distro's logo if its there.

Default config: 
```
small_pic 		= path to image
big_pic   		= distro or path to image
small_max_h     = maximum small image height in pixels
small_max_w     = maximum small image width in pixels
big_max_h       = maximum big image height in pixels
big_max_w       = maximum big image width in pixels
spin            = bool
rotation_period = seconds per full rotation

top_text  		= string
hostname  		= bool
kernel    		= bool
date      		= strftime format
time      		= strftime format
battery			= bool
brightness		= bool
```


## Screenshots
### Default config
![Default](./assets/default.png)

### Using custom ppms
![Custom](./assets/custom.png)

## Todo
- Try using DRM for KMSCON support?
- Anything else I find cool (gifs/animations)