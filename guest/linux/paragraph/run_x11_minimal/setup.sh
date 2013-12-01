sudo mkdir /lib64
cd /opt
ln -s /home/tc/new-x11/x11 .
export LD_LIBRARY_PATH=/opt/x11/build/bin:/opt/x11/build/lib
export DISPLAY=:0
sudo cp /opt/x11/build/bin/ld* /lib64
export PATH=$PATH:/opt/x11/build/bin 
sudo cp /opt/x11/build/bin/*.so* /lib64
sudo cp /opt/x11/build/lib/libXmuu.so.1 /lib64
sudo cp /opt/x11/build/lib/libxkbfile.so.1 /lib64
sudo cp /opt/x11/build/bin/mcookie /usr/bin
