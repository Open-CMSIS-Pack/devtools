========================================================================
    CMSIS_Build
========================================================================

***** How to build and debug this Linux project remotely: *****

In your Linux box symlink your repo into /devtools
For example: ln -s /mnt/c/devtools /devtools

Make sure the remote linux SSH server is running:
sudo service ssh --full-restart

Add the remote system connection in the menu Tools > Options > Cross Platform > Connection Manager

Add the connection to the Project Properties > General > Remote Build Machine