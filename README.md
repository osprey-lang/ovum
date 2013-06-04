# Ovum

**NOTE:** Ovum is currently undergoing a large refactoring. It does not compile.

Ovum is the virtual machine that runs programs written in [Osprey](http://bitbucket.org/Heurlinius/osprey). This is currently _not_ a pure C++ implementation, and will not compile outside Windows; there are several calls to various Windows API functions.

The Ovum solution consists of three projects: Ovum, VM and aves.

## The Ovum project

This is the main executable project. It is responsible for parsing VM arguments and for passing control to the VM.

## The VM project

This project contains the virtual machine implementation, and is responsible for loading modules (and native libraries), maintaining the garbage collector, and providing APIs for native libraries to interface with.

## The aves project

This is the standard library of Ovum/Osprey. It is written partly in C++ (for the things that must be implemented natively), partly in Osprey. To compile it, you must first compile the native library, then compile the Osprey module, linking with the DLL file you just created.

The module produced by this project is required by the [Osprey compiler](http://bitbucket.org/Heurlinius/osprey), unless you specifically compile with the `/nostdlib` option. Incidentally, aves should be compiled with the `/nostdlib` option.

### Why "aves"?

[Because birds](http://en.wikipedia.org/wiki/Aves).