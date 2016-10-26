#CRETE User Guide

## 1. Prerequisites
### Terminology
1. Virtual Machine (VM)
>The VM is what runs the guest OS. It's purpose is to emulate a physical
machine.

1. Host Operating System (host OS)
> The _host OS_ is the primary OS where CRETE will be built and executed from.

1. Guest Operating System (guest OS)
> The _guest OS_ is the OS that runs on the virtual machine.

### Unix/Linux Knowledge
A modest familiarity with Unix-style systems is required. You must be able to
make your way around the system in a terminal and interact with files from
command line.

### Operating System

CRETE requires the use of [Ubuntu
12.04-amd64](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-desktop-amd64.iso)
or [Ubuntu
14.04-amd64](http://releases.ubuntu.com/14.04/ubuntu-14.04.5-desktop-amd64.iso)
for the host OS. While various guest OS should work, only [Ubuntu
12.04-i386](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-server-i386.iso),
[ubuntu
12.04-amd64](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-server-amd64.iso)
and [Debian
7.11-i386](http://cdimage.debian.org/cdimage/archive/7.11.0/i386/iso-cd/debian-7.11.0-i386-netinst.iso)
have been tested.

## 2. Installing CRETE

Install the following packages required as dependencies for CRETE:
```bash
$ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
$ sudo apt-get update
$ sudo apt-get install build-essential libaio1 librbd1
$ sudo apt-get install --only-upgrade libstdc++6
```
Install CRETE from debian packages:
```bash
$ sudo dpkg -i crete-0.1.1-amd64.deb crete-native-qemu-0.1.1-amd64.deb
```
Pakcage __crete-0.1.1-amd64.deb__ provides executables and libraries of
CRETE. Package __crete-native-qemu-0.1.1-amd64.deb__ contains a native qemu that is used
for providing execution dependency and setting up tests for CRETE. On some
architectures, you might also need to set the following environment variables
(best to put them in a config file like .bashrc):
```bash
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```

CRETE can be removed by:
```bash
$ sudo apt-get remove crete crete-native-qemu
```

## 3. Preparing the Guest Operating System

The front-end of CRETE is an instrumented virtual machine (crete-qemu). You need
to setup a QEMU-compatible VM image to performa a certain test upon
CRETE. To get best performance, native qemu with kvm enabled is recommended for
all setups on guest VM image.

### Create a QEMU Image

```bash
$ qemu-img create -f qcow2 <img-name>.img <img-size>G
```

Where &lt;img-name&gt; is the desired name of your image, and &lt;img-size&gt;
is the upper bound of how large the image can grow to in Gigabytes. See [this
page](http://en.wikibooks.org/wiki/QEMU/Images#Creating_an_image) for more
details.

### Install the Guest OS

```bash
$ crete-native-qemu-system-x86_64 -hda <img-name>.img -m <memory> -k en-us -enable-kvm -cdrom <iso-name>.iso -boot d
```
Where &lt;memory&gt; is the amount of RAM (Megabytes), &lt;img-name&gt; is the
name of the image, and &lt;iso-name&gt; is the name of the .iso. The .iso, for
example, can be downloaded [here](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-server-amd64.iso). From this
point, follow the installation procedure to install the OS to the image.

See [this page](http://wiki.qemu.org/download/qemu-doc.html#sec_005finvocation) for
more boot options.

### Boot the OS

Once the OS is installed to the image, it can be booted with:

```bash
$ crete-native-qemu-system-x86_64 -hda <img-name>.img -m <memory> -k en-us -enable-kvm

```

Where &lt;memory&gt; is the amount of RAM (megabytes), &lt;img-name&gt; is the name of the image.

>#### Note

>If Booting Ubuntu 12.04 hangs, first boot to recovery mode, then resume to
 normal boot. This is likely caused by driver display problems in Ubuntu.

### Build CRETE-GUEST on the Guest OS
Install dependencies on the guest OS at first:
```bash
$ sudo apt-get update
$ sudo apt-get install build-essential cmake libelf-dev -y
```

Compile CRETE utilties on the guest OS:

```bash
$ scp -r <host-user-name>@10.0.2.2:</path/to/crete/guest> .
$ mkdir guest-build
$ cd guest-build
$ cmake ../guest
$ make
```
#### Other Guest OS Setup
Address Space Layout Randomization (ASLR) must be disabled on account of the
need for program addresses to remain consistent across executions/iterations. To
disable ASLR (Ubuntu 12.04):

```bash
echo "" | sudo tee --append /etc/sysctl.conf
echo '# Added by CRETE' | sudo tee --append /etc/sysctl.conf
echo '# Disabling ASLR:' | sudo tee --append /etc/sysctl.conf
echo 'kernel.randomize_va_space = 0' | sudo tee --append /etc/sysctl.conf
```

Set the following enviroment variables for using crete guest utilities (the
following script works on ubuntu 12.04 and assumes the "guest-build" locates
at the home folder):
```bash
echo ""  >> ~/.bashrc
echo '# Added by CRETE' >> ~/.bashrc
echo 'export PATH=$PATH:~/guest-build/bin' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/guest-build/bin'  >> ~/.bashrc
echo 'export C_INCLUDE_PATH=$C_INCLUDE_PATH:~/guest/lib/include'  >> ~/.bashrc
echo 'export LIBRARY_PATH=$LIBRARY_PATH:~/guest-build/bin'  >> ~/.bashrc
echo 'export CRETE_RAMDISK_PATH=~/tests/ramdisk'  >> ~/.bashrc
echo 'export LD_BIND_NOW=1'  | sudo tee --append /etc/sysctl.conf
```

Now, the guest OS is all set for using CRETE.

>### Tips for Using QEMU
When QEMU is running, it provides a monitor console for interacting with
QEMU. The monitor can be accessed from within QEMU by using the hotkey
combinations _ctrl+alt+2_, while _ctrl+alt+1_ allows you to switch back to the
Guest OS. One of the most useful functionality of the monitor is saving and
loading shopshots.

>####Save Snapshot
>Swith to monitor by pressing _ctrl+alt+2_ and use command:
```bash
$ savevm <snapshot-name>
```
>Where &lt;snapshot-name&gt; is the name you want to identify the snapshot by.

>####Load Snapshot
>To load a snapshot while launching QEMU from the host OS:
```bash
$ crete-native-qemu-system-x86_64 -hda <img-name>.img -m <memory> -k en-us -loadvm <snapshot-name>
```
>Note that the boot command of QEMU that loads a snopshot has to stay consistant with
the boot command of QEMU while saving snopshot, such as using the same <memory>
on the same image. Also, saving and loading snapshots cannot be done across
non-kvm and kvm modes.

>#### File Transfer Between the Guest and Host OS

>The simplest way to transfer files to and from the guest OS is with the _scp_ command.

>The host OS has a special address in the guest OS: **10.0.2.2**

>Here's an example that copies a file from the host to the guest:
```bash
scp <host-user-name>@10.0.2.2:<path-to-source-file> <path-to-destination-file>
```
>#### Gracefully Closing the VM
>When working with snapshots, it's often easiest to save a snapshot and close
 the VM from the monitor console rather than resorting to killing it or shutting
 it down. To gracefully close the VM:
```
ctrl+alt+2
q
<enter>
```
>Where &lt;enter&gt; means depress the _Enter_ key on your keyboard.

>#### Warning
>Try to aviod running more than one VM instance on a given image at a time, as
it may cause image corruption.

## 4. Usage
TBA
