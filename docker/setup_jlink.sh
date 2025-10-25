#!/bin/sh
set -e
set -x


apt-get update 
apt-get -y upgrade 

#
# The JLink package needs this dummy
# 
echo '#!/bin/bash\necho not running udevadm "$@"' > /usr/bin/udevadm 
chmod +x /usr/bin/udevadm


cd /tmp 
# Fetch the JLink software
wget --post-data "accept_license_agreement=accepted" https://www.segger.com/downloads/jlink/JLink_Linux_x86_64.deb
apt-get -y install --no-install-recommends ./JLink_Linux_x86_64.deb
rm ./JLink_Linux_x86_64.deb

apt-get clean 
rm -rf /var/lib/apt/lists/*
