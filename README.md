# CHAOS

Chaos is a virtualization toolstack.

The project is composed of a library, called `libh2`, that provides all
necessary functionality required to create, destroy and generally manage
virtual machines, and a set of applications, namely `chaos`, that implement a
user interface through a CLI or eventually other mechanisms.

# Build

## Dependencies

To build chaos you need to install the following dependencies:

* libjansson

You will also need Xen 4.7 built from source. Unfortunately it isn't possible
to build libh2 from installed xen headers due to dependencies of libxc that
aren't installed.


## Build

1. Run `make configure`
2. Edit `.config` appropriately
3. Run `make`

Note: the makefile has targets to build only `chaos` or `libh2`. However
`make chaos` doesn't directly depend on libh2 so you need to make the lib
first. See the Makefile for details.


# Install

After build just run `sudo make install`.
