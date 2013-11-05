# Ovum

**NOTE:** Ovum is currently very much in development. Although it successfully runs all [Osprey][osp] programs I have tested, it probably has a lot of memory issues and bugs all over the place. It is not ready for primetime.

Ovum is the virtual machine that runs programs written in [Osprey][osp]. This is _not_ a pure C++ implementation, and will not compile outside Windows; there are several calls to various Windows API functions. If you want to try compiling this project outside of Visual Studio 2012, good luck to you!

The Ovum solution consists of three projects: Ovum, VM and aves.

## The Ovum project

This is the main executable project. It is responsible for parsing VM arguments and for passing control to the VM.

### Why “Ovum”?

This VM was originally going to be called “OVM”, for “Osprey Virtual Machine”, but then I realised I would end up reading it as “ovum” anyway, so that became its official name. Also the word “ovum” is Latin for “egg”, which seems fitting given <del>my obsession with birds</del> the overall bird theme of this language and its VM.

## The VM project

This project contains the virtual machine implementation, and is responsible for loading modules (and native libraries), maintaining the garbage collector, and providing APIs for native libraries to interface with.

## The aves project

This is the standard library of Ovum/Osprey. It is written partly in C++ (for the things that must be implemented natively), partly in Osprey. To compile it, you must first compile the native library, then compile the Osprey module, linking with the DLL file you just created.

The module produced by this project is required by the [Osprey compiler][osp], unless you specifically compile with the `/nostdlib` option. Incidentally, aves should be compiled with the `/nostdlib` option.

### Why “aves”?

[Because birds](http://en.wikipedia.org/wiki/Aves).


  [osp]: http://bitbucket.org/Heurlinius/osprey