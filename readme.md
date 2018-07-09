# What it is

The monitor is a service that runs in the background keeping your server operational.

When something goes wrong, it can alert you in a variety of ways and report the full error log.

There is a bit of configuration to do, but it's mainly just pointing out where the logfiles will be.

Additionally there are a few bells and whistles. The monitor can be configured to watch the local
filesystem and report any changes. It can also report to a web-interface called the Watch, where
processor utilization and memory usage can be checked.

# Configuration

Details are in config.txt.

# Installation

CMake is the preferred build system. Build scripts have been provided.
Details are in INSTALL. Make sure to read it or your build will fail.

# Credits

The Monitor was developed at AlphaSheets by Scott Powell.

# License

This is a creative commons work.


