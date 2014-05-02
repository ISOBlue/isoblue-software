# ISOBlue Setup Tutorial #

## Getting Started ##
Refer to the BeagleBone document [here][bb]
for help connecting the BeagleBone Black to a computer
and installing the necessary drivers.
[bb]: http://beagleboard.org/static/beaglebone/latest/README.htm#step1

## Command Syntax ##
For the examples shown in this document,
lines starting with `$`, `#`, or `>` are commands.
To run the commands,
type everything after that first character into a terminal.

## Updating Angstrom ##
Download the latest Angstrom image from [here][angstrom].
Be sure to download the image for BeagleBone ( **NOT BeagleBone Black** )
if you want to use a microSD card (recommended).
Once you have the image, you need to write it to a micoSD card.
See the sections below based on the operating system of your computer.
[angstrom]: http://beagleboard.org/latest-images/ "BeagleBoard Latest Images"

### Linux (Tested in Ubuntu) ###
First you must identify which device your SD card is.
This can be done with the `lsblk` command.
Run `lsblk -d` without the SD card plugged in,
plug in the SD card, then run `lsblk -d` again.
The second time there should be a new line corresponding to the SD card.
Example output is below:
```shell
$ lsblk -d
NAME MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
sda    8:0    0 238.5G  0 disk
$ lsblk -d
NAME MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
sda    8:0    0 238.5G  0 disk 
sdb    8:16   1   3.7G  0 disk
```
You need what is in the `NAME` column, which is this example is `sdb`.
Replace occurrences of `DEVICE` with the device of your SD card
(`sdb` in the example).
Replace occurrences of `IMAGE` with the file you saved the Angstrom image as.
```shell
$ sudo umount /dev/DEVICE*
$ xz -dc IMAGE | sudo dd of=/dev/DEVICE
```

### Windows ###
See [here][bbupdate] for Windows directions.
[bbupdate]: http://beagleboard.org/static/beaglebone/latest/README.htm#update

## Assembly ##
![Assembled ISOBlue][bimg]
[bimg]: http://img825.imageshack.us/img825/5409/f2d8.jpg "Assembled ISOBlue"

### Bluetooth ###
Plug the Bluetooth dongle into the BeagleBone Black's USB port.

### CAN Cape ###
Attach the TT3201 CAN Cape to BeagleBone Black as in the above picture.
It really only fits on one way.

The cape has an 8 pin connector with rectangular holes on the top
and circular hole on the bottom.
Wires are attached by pushing them into the round holes.
Wires can be removed by pushing something such as a paper clip
into the rectangular hole above the wire,
***DO NOT*** try to just pull the wires out.
Pin 1 of the connector is indicated on the cape, and in the picture above.
When looking at the connector head on,
pin 1 is the farthest left, and pin 8 is the farthest right.
The table below shows which ISOBUS signals correspond to each pin.

| Pin | Signal          |
| --- | --------------- |
| 1   | ECU_GND         |
| 2   | NC              |
| 3   | NC              |
| 4   | NC              |
| 5   | Tractor CAN_L   |
| 6   | Tractor CAN_H   |
| 7   | Implement CAN_L |
| 8   | Implement CAN_H |

There are 3 DIP switches on the cape, directly to the left of the connector.
These switches enable/disable termination resistors on the CAN buses.
They ***MUST*** be in the off position,
which is toward the side of the BeagleBone Black that has the USB port
and the SD card slot.

### SD Card ###
Insert the microSD card with Angstrom into the BeagleBone Black's SD slot.

## Angstrom Setup ##
Connect the BeagleBone Black to a computer with a Mini-USB cable.
Also connect the BeagleBone Black to the Internet with an Ethernet cable.
You can now SSH into the BeagleBone Black with
IP 192.168.7.2 user root and no password.
If you do not have an SSH client, you can go [here][gateone]
with the browser of the computer to which you connected the BeagleBone Black.
This is where you run commands on the BeagleBone Black.
[gateone]: https://192.168.7.2 "GateOne SSH Client"

### Create a Password ###
It is **strongly advised** that you create a password for the BeagleBone Black.
This can be done with the `passwd` command.
Run the command and enter your desired password when prompted.
What you type will not show, just type the password anyway.
An example is shown below.
```shell
# passwd
Enter new UNIX password:
Retype new UNIX password:
passwd: password updated successfully
```

### Install Packages ###
There are a few packages needed to get the files, and to compile them.
The following commands will install and configure them.
```shell
# opkg update
# opkg install wget git kernel-dev bluez4-dev
# make -C /usr/src/kernel scripts
# ln -fs /usr/src/kernel /lib/modules/`uname -r`/build
```

#### Install LevelDB ####
LevelDB is also needed, but cannot currently be installed with `opkg`.
You must download the source, compile, and install it as below.

##### Clone the LevelDB git repo #####
```shell
$ git clone https://code.google.com/p/leveldb/ ~/leveldb
```

##### Compile LevelDB #####
```shell
$ cd ~/leveldb
$ make
```

##### Install LevelDB #####
```shell
# cp --preserve=links libleveldb.* /usr/local/lib
# cp -r include/leveldb /usr/local/include/
# ldconfig
```

### TowerTech Patches ###
At the time this was written,
the CAN cape needed changes to the kernel and some modules.
If those changes have made it upstream,
this section is not longer needed.
After you have bought the cape from TowerTech,
email them and they will send you a link to a new kernel and modules.
To install them run the commands below,
replacing `LINK` with the link you got from TowerTech.
```shell
# wget --no-check-certificate -O - LINK | tar -zx -C /
# reboot
```
The BeagleBone Black will restart.
After is has restarted, you will be able to SSH in again and continue.
The next few commands are so that the source of the regular Angstrom kernel
will be used to compile modules.
This is needed because the patched kernel does not come with its source.
```shell
# ln -fs /usr/src/kernel /lib/modules/`uname -r`/build
# ln -fs /lib/modules/`cat /usr/src/kernel/kernel-abiversion`/extra \
> /lib/modules/`uname -r`/extra
```

### Enable Bluetooth ###
To enable Bluetooth a file on the BeagleBone Black needs to be edited.
In the file `/var/lib/connman/settings` there is a line about Bluetooth
which is set to false,
you must change it to true.
An example of the file's contents is shown below.
```shell
$ cat /var/lib/connman/settings
[global]
Timeservers=0.angstrom.pool.ntp.org;1.angstrom.pool.ntp.org;2.angstrom.pool.ntp.org;3.angstrom.pool.ntp.org
OfflineMode=false

[Wired]
Enable=true

[WiFi]
Enable=true
[Bluetooth]
Enable=false
```
The line to change happens to be the last one here.
Under the heading Bluetooth, the line should be `Enable=true`.
You can edit the file with the following command:
```shell
# nano /var/lib/connman/settings
```
Make the necessary change, then hit Ctrl-O, Enter, Ctrl-X.
For the change to take effect, you must restart the BeagleBone Black.
```shell
# reboot
```
## ISOBlue Installation ##

### Clone the Git Repo ###
The code from this repository need to be on the BeagleBone Black.
The below command clones the repository onto it.
```shell
$ git clone git://github.com/ISOBlue/isoblue-software.git ~/isoblue-software
```

### Compile and Install SocketCAN Modules ###
The following commands compile, and then install,
the kernel modules need to use ISOBUS in SocketCAN.
```shell
$ cd ~/isoblue-software/socketcan-isobus
$ make modules
# make modules_install
# depmod -a
```

### Compile and Install ISOBlue tools ###
The following commands compile, and then install,
all the processes for running or testing ISOBlue.
```shell
$ cd ~/isoblue-software/tools
$ make
# make install
```

### Install Startup Files ###
The following commands install the startup files that tell the BeagleBone Black
how to configure itself on boot.
```shell
$ cd ~/isoblue-software/angstrom
# cp systemd/* /etc/systemd/system/
# systemctl enable isoblue.target
# cp udev/* /etc/udev/rules.d/
```

## Android Library ##
ISOBlue is intended to be used with and Anrdoid library.
That library along with its source code, description, and usage is [here][lib].
[lib]: https://github.com/ISOBlue/isoblue-android/tree/master/libISOBlue "libISOBlue"
 
## Usage Examples ##

### Using without an ISOBUS Connection ###
ISOBlue needs to be connected to a valid ISOBUS network,
even to send messages from one program to another on the BeagleBone Black.
If you would like to try the example below without connecting to an ISOBUS,
you must run the following commands to put ISOBlue in loopback mode.
```shell
# ifconfig ib_eng down
# canconfig ib_eng ctrlmode loopback on
# ifconfig ib_eng up
# ifconfig ib_imp down
# canconfig ib_imp ctrlmode loopback on
# ifconfig ib_imp up
```
This mode will persist until you restart the BeagleBone Black.

### SocketCAN Module ###
ISOBUS message can be sent and received using SocketCAN.
A simple example program is sc_mod_test,
with its source code [here][sc_mod_test].
The syntax for calling it is below.
Replace `DEVICE` with the CAN device to use
(for ISOBlue there are *ib_eng* and *ib_imp*).
Replace `[ADDR]` with a preferred address for the program to claim,
or leave it out to self configure.
```shell
$ ~/isoblue-software/tools/sc_mod_test DEVICE [ADDR]
```
The program sends a request PGN then listens for messages,
printing any it sees to the terminal.
[sc_mod_test]: ../tools/sc_mod_test.c "ISOBUS SocketCAN Module Test"

