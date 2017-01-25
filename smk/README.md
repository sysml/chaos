# SimpleMaKe: simple build system based on GNU make

SimpleMaKe intends to provide a simple and straightforward build system for C
and C++ projects. It provides all the boilerplate for the most common targets
leaving the user with only the simple task of defining the binaries and/or
libraries to be built, and the objects they depend on.

This is not intended to build complex projects with complex structures or
complex configuration or build needs. Rather it targets simple projects that
use simple and standard build mechanisms, and a project structure defined in
the next section.

## Project structure

SimpleMaKe based projects have always the same base directory structure:

```
.
├── bin : sources for binaries
├── inc : headers for libraries
├── lib : sources for libraries
├── smk : SimpleMaKe sources
└── Makefile
```

The idea behind this scheme is to always provide most of the functionality as
a library that is then used by one or more very simple applications.
Applications have only the simple tasks of reading command line arguments and/or
configurations files, and implementing some sort of main loop that mostly calls
libraries, being therefore relatively simple. Application-specific headers
(e.g., headers relative to configuration parsing) should be placed under `bin/`
and imported by the application with `""` as opposed to `<>`.

Although the idea is to always write most of the functionality as a library you
don't have to build it as a library; instead you can build everything as a
static binary. The reference text below explains the multiple ways you can use
smk.

## Basic targets

SimpleMaKe provides the following targets:

* **all**: build the project
* **install**: install project's binaries and libraries on the system
* **uninstall**: uninstall project from the system
* **clean**: remove generated files except final binaries and libraries
* **distclean**: remove all generated files except `.config`
* **properclean**: remove all generated files including `.config`
* **configure**: Run a configuration script

The system also defines targets for each of the binaries and libraries defined.


# Usage

## Basic usage

There are two main steps to use SimpleMaKe:

1. Include `s.mk`
2. Define targets using `smk_*` functions

The most trivial Makefile looks like:

```
include smk/s.mk

$(eval $(call smk_binary,app,bin/app.o))
```

This example Makefile would be able to build a binary called `app` by
compiling and linking `app.c` located under `bin/` directory.


## SimpleMaKe reference

SimpleMaKe defines a set of functions that let you define what to build and a
set of configuration variables to configure the behaviour of the system.

Please define the configuration variables before `include smk/s.mk`. To call
smk functions please use the following construct
`$(eval $(call smk_<fn>,<args>))` appropriately replacing `<fn>` and `<args>`.

### SimpleMaKe configuration

`verbose`  : set to `y` to print verbose commands.

`smk_lang` : project language, one of `c` or `c++`. Default: `c`.

`smk_dir`  : smk directory. Default: `smk`.

`smk_conf_file`   : smk configuration file. Default: `.config`.

`smk_conf_script` : configuration script. Default: `./configure`.

### SimpleMaKe functions

`smk_binary (binary, objects)`

* `binary`  : the binary name
* `objects` : space separated list of object paths

Define a binary to be built.

Example usage: `$(eval $(call smk_binary,app,bin/app.o))`

`smk_library (library, v_major, v_minor, v_bug, objects)`

* `library` : the library name
* `v_major` : library version major number
* `v_minor` : library version minor number
* `v_bug`   : library version bug revision
* `objects` : space separated list of object paths

Define a library to be built.

Example usage: `$(eval $(call smk_library,apl,0,1,0,lib/apl.o))`

`smk_depends (binary,library)`

* `binary`  : the binary name
* `library` : the library name

Define a dependency of a project binary on a project library. Notice that this
function won't add compilation flags for linking the binary with the library,
you need to add those manually.

Example usage: `$(eval $(call smk_depend,app,apl))`


# SimpleMaKe based Makefile example

Let's now define a more complex Makfile to build an application called app,
comprised of two source files, as well as a library called apl, comprised of
two more source files. The following code defines the dependency between the
two and sets a few build flags.

```
include smk/s.mk

app_obj     :=
app_obj     += bin/app.o
app_obj     += bin/cmdline.o

apl_obj     :=
apl_obj     += lib/structs.o
apl_obj     += lib/business.o


$(eval $(call smk_binary,app,$(app_obj)))
$(eval $(call smk_depend,app,apl))

$(apl_obj): CFLAGS += -DAPP_CONFIG_1
$(apl_bin): LDFLAGS += -lapl

$(eval $(call smk_library,apl,$(apl_obj)))
```

And that's all you need to have your build system ready.
