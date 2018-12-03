# EOS IPTV MW
EOS is simple IPTV MW prepared mainly for Android AOSP environment.
It is a framework which can be easily ported to a target platform. 

## Building on Linux
Linux port of Cron player is based on FFMPEG, ALSA and X11 (via XV).
It was created for prototyping and functional demo, so it does not have all functionalities but it should give a good starting point for a Cron player implementation.

Few packages are needed to have test application built on Linux.
For Ubuntu you should install:
 - libavformat-dev
 - libswscale-dev
 - libavresample-dev
 - libasound2-dev
 - libx11-dev
 - libxtst-dev

## Building on Android 
The project is prepared for a build in AOSP build three. There was no intention to create an NDK friendly build, but it should not be a big deal to enable it.

EOS is buildable but it has no functional Cron player for Android, as this is platform specific.
It contains only dummy player which is just consuming the data and not really playing it.

As Android.mk is already prepared, just place the source code in the AOSP source three and run 'mm'.
To enable it in full build, please add three packages to your product make file:
**For example in device.mk**

	PRODUCT_PACKAGES += \
	    libeos \
	    libeos_jni \
	    eos_cmd

## Running test app
Just run eos_test from bin directory on Linux or on the platform and you will enter internal test app command line interface.
You can run 'help' command for details on which commands are supported.
**Example of file playback:**

		start main file:///data/srf1_dump_sd.ts

## Getting Java API JAR file
During Linux build the Java JAR file will be available in the bin directory (bin/eos.jar).
Other option is to manually build it with 'ant -q -S -f src/java/build.xml jar' command from the project root. 
