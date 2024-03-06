# baseqware-v3

Simple cross platform kernel driver/module based CS2 cheat rendered in the Steam overlay browser window.

## Structure

The project is comprised of 3 primary components:

- Web client responsible for rendering elements on the screen, as well as the configuration menus
- Server for the web client, contains the actual cheat logic
- The kernel driver that is used to perform queries on the game process

## Usage

IMPORTANT: Don't use this or any other software that grants an unfair advantage without `-insecure` as a launch option! Using these programs on official game servers is prohibited and will result in your account getting banned.

### Windows

- [Enable test signing mode](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option)
- Put your offsets into the `offsets` directory (https://github.com/a2x/cs2-dumper on Windows or https://github.com/Albert24GG/cs2-dumper/tree/linux on Linux)
- Compile the kernel driver and the server
- Place the resulting `SIoctl.sys` file in the same directory as the server executable
- Run the server as administrator, it will automatically install/load and stop/delete the driver during execution
- (Re)start the game, only use with `-insecure` as a launch option!
- The cheat can be stopped whenever by pressing the enter key in the associated console window
- Fire up your web server of choice (e.g. python's http.server) in the `baseqware-client` directory
- Open the Steam overlay from in-game, go to the web page
- Pin the browser window and minimize its opacity
- Click inside the tab (anywhere) once
- Close the overlay

For development and debugging: in the Windows driver component there is also a standalone executable for loading and unloading the driver which can be used by starting it before the game, after which running the server will not cause unloading upon termination, which means the cheat can be reloaded by restarting the server executable without having to reload the game.

## Features

- BoxESP
