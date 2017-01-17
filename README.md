#CRETE User Guide

[![Build Status](https://travis-ci.org/SVL-PSU/crete-dev.svg?branch=master)](https://travis-ci.org/SVL-PSU/crete-dev)

## 1. Prerequisites
### 1.1. Terminology
Virtual Machine (VM)
>The VM is what runs the guest OS. Its purpose is to emulate a physical
machine.

Host Operating System (host OS)
> The _host OS_ is the primary OS where CRETE will be built, installed and executed from.

Guest Operating System (guest OS)
> The _guest OS_ is the OS that runs on the virtual machine.

### 1.2. Unix/Linux Knowledge
A modest familiarity with Unix-style systems is required. You must be able to
make your way around the system in a terminal and interact with files from
command line.

### 1.3. Operating System

CRETE requires the use of [Ubuntu
12.04-amd64](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-desktop-amd64.iso)
or [Ubuntu
14.04-amd64](http://releases.ubuntu.com/14.04/ubuntu-14.04.5-desktop-amd64.iso)
for the host OS. While various guest OS should work, only
[Ubuntu-12.04.5-server-i386](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-server-i386.iso),
[Ubuntu-12.04.5-server-amd64](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-server-amd64.iso),
[Ubuntu-14.04.5-server-amd64](http://releases.ubuntu.com/14.04/ubuntu-14.04.5-server-amd64.iso),
and [Debian-7.11.0-i386](http://cdimage.debian.org/cdimage/archive/7.11.0/i386/iso-cd/debian-7.11.0-i386-netinst.iso)
have been tested.

## 2. Installing CRETE from Debian Package

Install the following dependencies at first:
```bash
$ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
$ sudo apt-get update
$ sudo apt-get install build-essential libaio1 librbd1 libsdl1.2debian
$ sudo apt-get install --only-upgrade libstdc++6
```
Install CRETE from debian packages:
```bash
$ sudo dpkg -i crete-0.1.1-amd64.deb crete-native-qemu-0.1.1-amd64.deb
```
Pakcage __crete-0.1.1-amd64.deb__ provides executables and libraries of
CRETE. Package __crete-native-qemu-0.1.1-amd64.deb__ contains a native qemu that
provides execution dependency and creates tests for CRETE.

On some architectures, you might also need to set the following environment
variables (best to put them in a config file like .bashrc):
```bash
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```

CRETE can be removed by:
```bash
$ sudo apt-get remove crete crete-native-qemu
```

## 3. Preparing the Guest Operating System
>#### Note

> You may skip this section by using a prepared VM image
  [crete-demo.img](http://svl13.cs.pdx.edu/crete-demo.img). This image has
  Ubuntu-14.04.5-server-amd64 installed as the Guest OS along with all CRETE
  guest utilities.

> Root username: __test__

> Password: __crete__

The front-end of CRETE is an instrumented VM (crete-qemu). You need
to setup a QEMU-compatible VM image to performa a certain test upon
CRETE. To get best performance, native qemu with kvm enabled is recommended for
all setups on the guest VM image.

### 3.1. Create a QEMU Image

```bash
$ qemu-img create -f qcow2 <img-name>.img <img-size>G
```

Where &lt;img-name&gt; is the desired name of your image, and &lt;img-size&gt;
is the upper bound of how large the image can grow to in Gigabytes. See [this
page](http://en.wikibooks.org/wiki/QEMU/Images#Creating_an_image) for more
details.

### 3.2. Install the Guest OS

```bash
$ crete-native-qemu-system-x86_64 -hda <img-name>.img -m <memory> -k en-us -enable-kvm -cdrom <iso-name>.iso -boot d
```
Where &lt;memory&gt; is the amount of RAM (Megabytes), &lt;img-name&gt; is the
name of the image, and &lt;iso-name&gt; is the name of the .iso. The iso of ubuntu-12.04.5-server-amd64, for
example, can be downloaded [here](http://releases.ubuntu.com/12.04/ubuntu-12.04.5-server-amd64.iso). From this
point, follow the installation procedure to install the OS to the image.

See [this page](http://wiki.qemu.org/download/qemu-doc.html#sec_005finvocation) for
more boot options.

### 3.3. Boot the OS

Once the OS is installed to the image, it can be booted with:

```bash
$ crete-native-qemu-system-x86_64 -hda <img-name>.img -m <memory> -k en-us -enable-kvm

```

Where &lt;memory&gt; is the amount of RAM (megabytes), &lt;img-name&gt; is the name of the image.

>#### Note

>If Booting Ubuntu 12.04 hangs, first boot to recovery mode, then resume to
 normal boot. This is likely caused by driver display problems in Ubuntu.

### 3.4. Build CRETE Guest Utilities
Install the following dependencies on the guest OS at first:
```bash
$ sudo apt-get update
$ sudo apt-get install build-essential cmake libelf-dev libcap2-bin -y
```

Compile CRETE utilties on the guest OS:

```bash
$ scp -r <host-user-name>@10.0.2.2:</path/to/crete/guest> .
$ mkdir guest-build
$ cd guest-build
$ cmake ../guest
$ make
```
### 3.5. Other Guest OS Configurations
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

## 4. Generating Test Cases for Linux Binaries
This section will show how to use CRETE to generate test cases for unmodified Linux
binaries. In this section, I will use "crete-demo.img" as the VM image
prepared for CRETE. Also, I will use __echo__ from _GNU CoreUtils_ as the target
binary under test.

### 4.1 Setting-up the Test on the Guest OS
####Provide a configuration file for the target binary
Boot the VM image using native qemu without kvm-enabled first by:
```bash
$ crete-native-qemu-system-x86_64 -hda crete-demo.img -m 256 -k en-us
```
A sample configuration file, _crete.demo.echo.xml_, for _echo_ is given as follows :
```xml
<crete>
	<exec>/bin/echo</exec>
	<args>
		<arg index="1" size="8" value="abc" concolic="true"/>
	</args>
</crete>
```
A briefly explaination for each pertinent node is as follows (See _5. CRETE
Configuration Optionts_ for more information):
```xml
<exec>/bin/echo</exec>
```
This is the path to the executable under test.
```xml
<arg index="1" size="8" value="abc" concolic="true"/>
```
In this way, we will test the binary with one concolic argument
with size of 8 bytes and its initial value is "abc".

#### Start CRETE guest utility on the given setup
With the configuration file, we are ready to use _crete-qemu_ to start the test
on the target binary.

We take advantage of the snapshot functionality of qemu to boost the process of
booting the guest OS by using _crete-qemu_. We first save a snapshot and quit
from the native qemu by:
```bash
ctrl+alt+2
$ savevm test
enter
$ q
enter
```
From the host OS, luanch _crete-qeum_ by loading the snapshot we just saved:
```bash
$ crete-qemu-2.3-system-x86_64 -hda crete-demo.img -m 256 -k en-us -loadvm test
```

On the guest OS, execute CRETE guest utility with the guest configuration file:
```bash
$ crete-run -c crete.demo.echo.xml
```

Now, the guest OS is all set and should be waiting for CRETE back-end on the
Host OS to start.

### 4.2 Executing CRETE Back-end on the Host OS
CRETE back-end has three parts: _crete-vm-node_, for managing VM instances,
_crete-svm-node_, for managing symbolic VM instances, and _crete-dispatch_, for
coordinating the whole process.

####Start crete-dispatch on the Host OS:
A sample configuration file, _crete.dispatch.xml_, for _crete-dispatch_ is:
```xml
<crete>
    <mode>developer</mode>
    <vm>
        <arch>x64</arch>
    </vm>
    <svm>
        <args>
            <symbolic>
                --max-memory=1000
                --disable-inlining
                --use-forked-solver
                --max-sym-array-size=4096
                --max-instruction-time=5
                --max-time=150
                --randomize-fork=false
                --search=dfs
            </symbolic>
        </args>
    </svm>
    <test>
        <interval>
            <trace>10000</trace>
            <tc>10000</tc>
            <time>900</time>
        </interval>
    </test>
    <profile>
        <interval>10</interval>
    </profile>
</crete>
```
Start _crete-dispatch_ with the sample configuration file:
```bash
$ crete-dispatch -c crete.dispatch.xml
```
####Start crete-vm-node on the Host OS:
A sample configuration file, _crete.vm-node.xml_, for _crete-vm-node_ is:
```xml
<crete>
    <vm>
        <count>1</count>
    </vm>
    <master>
        <ip>localhost</ip>
        <port>10012</port>
    </master>
</crete>
```
Start _crete-vm-node_ with the sample configuration file (_crete_vm_node_ must
be started from the same folder where the _crete-qemu_ was started):
```bash
$ crete-vm-node -c crete.vm-node.xml
```
####Start crete-svm-node on the Host OS:
A sample configuration file, _crete.svm-node.xml_, for _crete-svm-node_ is:
```xml
<crete>
    <svm>
        <count>1</count>
    </svm>
    <master>
        <ip>localhost</ip>
        <port>10012</port>
    </master>
</crete>
```
Start _crete-svm-node_ with the sample configuration file:
```bash
$ crete-svm-node -c crete.svm-node.xml
```

### 4.3 Collecting Result on the Host OS
TBA

## 5. Configuration Options

Configuration is done within the guest OS via an XML file that is passed as an argument to _crete-run_.

Example:
```bash
crete-run -c crete.xml
```

The configuration file (crete.xml) may be arbitrarily named, but the contents must match the following structure (order does not matter).

```xml
<crete>
    <exec>string</exec>
    <args>
        <arg index="<uint>" size="<uint>" value="<string>" concolic="<bool>"/>
    </args>
    <files>
        <file path="<string>" size="<uint>" concolic="<bool>"/>
    </files>
    <stdin size="<uint>" value="<string>" concolic="<bool>"/>
</crete>
```

Item-by-item:

#### crete.exec
```xml
<exec>string</exec>
```
- Type: string.
- Description: the full path to the executable to be tested.
- Optional: no.

#### crete.args
```xml
<args></args>
```
Command line arguments to the executable are defined here.

There is a single default argument for index="0", as defined by the system (typically a path to the executable). By listing an _arg_ element for _index="0"_, you will overwrite this default argument.

#### crete.args.arg
```xml
<args>
    <arg index="<uint>" size="<uint>" value="<string>" concolic="<bool>"/>
</args>
```
index:
- Type: unsigned int.
- Description: index of argv[] that this element will apply to.
- Optional: no.

size:
- Type: unsigned int.
- Description: number of bytes of the argument __not__ including the null terminator.
- Optional: yes, if _value_ is given (from which _size_ can be derived).

value:
- Type: string.
- Description: the initial value of the argument (if concolic), or the constant
value of the argument (if not concolicp).
- Optional: yes, if _size_ is given.

concolic:
- Type: boolean
- Description: designate this argument to have test values generated for it.
- Optional: yes - defaults to _false_.

### crete-dispatch configuration
TBA
### crete-vm-node configuration
TBA
### crete-svm-node configuration
TBA

## 6. FAQ

### 6.1. Why is the VM not starting or misbehaving?

The most likely cause of this is the VM Image is corrupted.

There is no way to undo the damage. Forfeit the image, make a new one, and
backup regularly. Try to not run more than one VM instance on a given image at a
time.

### 6.2. Why I can't switch to QEMU monitor by using ctrl+alt+2?
A solution for this is forwarding the monitor to a local port through
telnet while launching qemu on the host OS:
```bash
$ crete-qemu-2.3-system-x86_64 -hda crete-demo.img -m 256 -k en-us -loadvm test -monitor telnet:127.0.0.1:1234,server,nowait
```
Use the following command to access QEMU's monitor on the host OS:
```bash
$ telnet 127.0.0.1 1234
```

### 6.3. Why is "apt-get install build-essential" not working from the guest OS?
To resolve this, use the following command from the guest OS:
```bash
$ sudo rm -rf /var/lib/apt/lists/*
```

## 7. Building CRETE

### 7.1 Dependencies

The following apt-get packages are required:
```bash
sudo apt-get update
sudo apt-get install build-essential lcov libcap-dev flex bison cmake libelf-dev subversion git libtool libpixman-1-dev
sudo apt-get build-dep qemu
```

If your host system is Ubuntu 14.04, you will need to revert to an older version of bison:
```bash
sudo apt-get remove libbison-dev bison # Remove current version of bison.
sudo apt-get update # For good measure.
wget https://launchpad.net/ubuntu/+source/bison/1:2.5.dfsg-2.1/+build/3270698/+files/libbison-dev_2.5.dfsg-2.1_amd64.deb
wget https://launchpad.net/ubuntu/+source/bison/1:2.5.dfsg-2.1/+build/3270698/+files/bison_2.5.dfsg-2.1_amd64.deb
# Install bison-2.5 which is required by the dependency STP.
sudo dpkg -i libbison-dev_2.5.dfsg-2.1_amd64.deb
sudo dpkg -i bison_2.5.dfsg-2.1_amd64.deb
sudo apt-mark hold libbison-dev bison # Prevent apt-get from updating.
```

### 7.2 Source Tree

Grab a copy of the source tree:
```bash
git clone https://github.com/SVL-PSU/crete-dev.git crete
```

### 7.3 Preparing the Environment
>#### Warning

> CRETE uses Boost 1.59.0. If any other version of Boost is installed on the system, there may be conflicts. It is recommended that you remove any conflicting Boost versions.

If you are using Ubuntu 14.04, you will need to make the compiler aware of the location of some header files.
The exact location will depend on your version of gcc. To find this location:
```bash
locate bits/c++config.h
```
If the result is, for example:
```bash
/usr/include/x86_64-linux-gnu/c++/4.8/bits/c++config.h
```
Then you would do, for example:
```bash
echo 'export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/usr/include/x86_64-linux-gnu/c++/4.8' >> ~/.bashrc
source ~/.bashrc
```

### 7.4 Building

>#### Note

> CRETE requires a C++11 compatible compiler.
> We recommend clang-3.2 or g++-4.9 or higher versions of these compilers.

From outside of CRETE's top-level directory:
```bash
mkdir crete-build
cd crete-build
cmake ../crete -DCMAKE_CXX_COMPILER=/path/to/c++11/compatible/compiler
make # This will take a while; do NOT use -j
```

If you like, you can add the executables and libraries to your .bashrc file:
```bash
echo '# Added by CRETE' >> ~/.bashrc
echo export PATH='$PATH':`readlink -f ./bin` >> ~/.bashrc
echo export LD_LIBRARY_PATH='$LD_LIBRARY_PATH':`readlink -f ./bin` >> ~/.bashrc
echo export LD_LIBRARY_PATH='$LD_LIBRARY_PATH':`readlink -f ./bin/boost` >> ~/.bashrc
source ~/.bashrc
```

At this point, you're all set!
