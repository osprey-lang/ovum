# Ovum

**NOTE:** Ovum is currently very much in development. Although it successfully runs all [Osprey][osp] programs I have tested, it probably has a lot of memory issues and bugs all over the place. It is not ready for primetime.

Ovum is the virtual machine that runs programs written in [Osprey][osp]. For the time being, this is _not_ a pure C++ implementation, and will not compile outside Windows; there are several calls to various Windows API functions. If you want to try compiling this project outside of Visual Studio 2012, good luck to you!

The Ovum solution consists of four projects: Ovum, VM, aves and aves.tests.

## The Ovum project

This is the main executable project. It is responsible for parsing VM arguments and for passing control to the VM.

### Why “Ovum”?

This VM was originally going to be called “OVM”, for “Osprey Virtual Machine”, but then I realised I would end up reading it as “ovum” anyway, so that became its official name. Also the word “ovum” is Latin for “egg”, which seems fitting given <del>my obsession with birds</del> the overall bird theme of this language and its VM.

## The VM project

This project contains the virtual machine implementation, and is responsible for loading modules (and native libraries), maintaining the garbage collector, providing APIs for native libraries to interface with, as well as executing bytecode.

## The aves project

This is the standard library of Ovum/Osprey. It is written partly in C++ (for the things that must be implemented natively), partly in Osprey. To compile it, you must first compile the native library, then compile the Osprey module, linking with the DLL file you just created.

The module produced by this project is required by the [Osprey compiler][osp], unless you specifically compile with the `/nostdlib` option. Incidentally, aves should be compiled with the `/nostdlib` option.

In fact, aves should be compiled with the following options (assuming the working directory is the repository’s `aves/osp`):

    /nativelib DLLDIR\aves.dll /meta meta.txt /type module /nostdlib /out OUTDIR\aves.ovm /doc OUTDIR\aves.ovm.json aves.osp

where `DLLDIR` is the directory containing `aves.dll`, and `OUTDIR` is the output directory.

### Why “aves”?

[Because birds](http://en.wikipedia.org/wiki/Aves).

## The aves.tests project

This pure-Osprey project contains unit tests for the public API of aves. It depends on the [`testing.unit`][testing.unit] module. This project is a work in progress; the vast majority of aves is *not* tested by it. However, it will be gradually expanded to cover as much as possible.

The aves.tests project should be compiled with the following options:

	/libpath LIBPATH /main aves.tests.main /r *.osp

where `LIBPATH` is the module library path. Note the use of `/r *.osp` to include all `.osp` files: unlike the aves project, this project does not contain a list of `use` directives that name all test files. This may be changed in the future.

When running the tests, the module can be invoked with any number of command-line arguments. If invoked with none, it will simply run all the tests in the module. Otherwise, each argument is treated as the name of a test class, without the `aves.tests.` prefix. For example,

	Run all tests:
	> ovum aves.tests.ovm

	Test only Buffer and BufferView:
	> ovum aves.tests.ovm BufferTests BufferViewTests

Usually, all tests should be run.


  [osp]: http://bitbucket.org/OspreyLang/osprey
  [testing.unit]: http://bitbucket.org/OspreyLang/testing.unit