# Environment controller

Here, we manage everything that has to do with the OS environment bootstrap for the system. It is important to note how
exactly the environment is managed under Aniva. We work with profiles that can contain variables, which determine how the environment
is shaped. The kernel provides a few base variables inside the Global profile. These give basic information about the system, like 
kernel name, version, root file system type, et cetera. Besides these basic variables, there is also a TOML interpreter inside every
build of the kernel (or at least a driver that can interpret TOML) which is used to load/save variables from/to files.

## The loading of custom variables

When the system is booting and it has found a root file system, it will look inside this file system for a file called aniva.cfg. This file contains
a TOML core with a small layer of platform specific file standard on top, in order to verify the file. The exact layout of this is documented inside the Aniva kernel
documentation. This file will ultimately contain any other variables that the system admin wants to have loaded. These are device-specific things, like
network configuration, paths to other kernel configuration files, binary search paths, library search paths, header search paths, et cetera.
