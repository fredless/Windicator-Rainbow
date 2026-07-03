# Windicator - Rainbow Edition

*Forked from Wielding/Windicator to suit my purposes, no longer maintained. I wanted something that showed a visual color distinguish so I could more quickly understand which desktop I was on. I was also having problems with the fork parent stopping silently from time to time, so I've tried to address that and make the new utility smaller overall.* 

This is a simple Windows 11 application that shows an indicator in the system tray displaying the current virtual
desktop number. It does not work on Windows 10 due to Microsoft changing the registry keys.

I believe it also requires at least the first Windows 11 feature update due to Microsoft changing the registry location for
the values once again.

The application maxes out at 10 virtual desktops since anything more than 1 digit is difficult to show nicely in the
tray icon. Desktop 10 is shown as '0' and Anything past that displays an '*'. If you use more than 10 virtual desktops I
think you might have a workflow problem to address.

The application monitors the registry for changes to your virtual desktops and then updates the notification icon with
the desktop number. It uses events to wait for registry changes so there is no activity in the background unless you
change desktops. It uses very little memory and virtually no CPU.

There is no installer for this application since it is a single Windows executable. Just put it wherever you want it and
run it. Or you can create a shortcut to it and place that in your shell:startup folder.

Each desktop number has its own distinct icon color, so you can recognize the desktop at a glance without
reading the number. That icon style is based on the blue icon set contributed to the fork parent     by
[Lisa Jackson](https://github.com/LisaJackson24-2).

![](resources/small_1.ico)
![](resources/small_2.ico)
![](resources/small_3.ico)
![](resources/small_4.ico)
![](resources/small_5.ico)

If you are just downloading the release executable you will need the latest Microsoft Visual C++ Redistributable if you
don't have it already. You can check if you already have it by just running the application. If you don't have the
runtime a dialog will pop up telling you that it is missing. If that happens the runtime can be downloaded directly from
[here](https://aka.ms/vs/17/release/vc_redist.x64.exe) or navigate to
https://support.microsoft.com/en-us/topic/the-latest-supported-visual-c-downloads-2647da03-1eea-4433-9aff-95f26a218cc0
and select the x64 download.

## Building

If you want to build it you need to make sure you have Visual Studio 2022 with the CMake tools installed. Otherwise, you
can download [cmake](https://github.com/Kitware/CMake) and the [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) to use another IDE (e.g. Visual Studio Code).
