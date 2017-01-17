# 1. install dependencies
sudo rm -rf /var/lib/apt/lists/*
sudo apt-get update
sudo apt-get install build-essential cmake  libelf-dev libcap2-bin -y

# 2. disable ASLR
echo "" | sudo tee --append /etc/sysctl.conf
echo '# Added by CRETE' | sudo tee --append /etc/sysctl.conf
echo '# Disabling ASLR:' | sudo tee --append /etc/sysctl.conf
echo 'kernel.randomize_va_space = 0' | sudo tee --append /etc/sysctl.conf

# 3. set environmental variable
echo ""  >> ~/.bashrc
echo '# Added by CRETE' >> ~/.bashrc
echo 'export LD_BIND_NOW=1'  | sudo tee --append /etc/sysctl.conf
echo 'export PATH=$PATH:~/guest-build/bin' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/guest-build/bin'  >> ~/.bashrc

echo 'export C_INCLUDE_PATH=$C_INCLUDE_PATH:~/guest/lib/include'  >> ~/.bashrc
echo 'export LIBRARY_PATH=$LIBRARY_PATH:~/guest-build/bin'  >> ~/.bashrc

########
Upgrade gcc to 4.8.1

sudo apt-get install python-software-properties
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install gcc-4.8
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 50

##### Compile 32 bits guest on 64 bits OS ######
## apt-get install gcc-multilib
## apt-get install g++-multilib
## apt-get install libelf-dev:i386
## Add "LIST(APPEND CMAKE_CXX_FLAGS -m32)" and "LIST(APPEND CMAKE_C_FLAGS -m32)" to gueset/CMakeLists.txt
