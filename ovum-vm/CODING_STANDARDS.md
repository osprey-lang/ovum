# C++ coding standards

***This document is a work in progress.***

The coding standards used in the C++ portion of Ovum differ slightly from many recommendations about C++. For the most part, this is caused by the necessity to remain `exception`-free. See [No exceptions](#no-exceptions) for more details.

Few parts of Ovum adhere to all of these standards fully. The code will be updated to reflect them as part of an ongoing process of cleaning stuff up.

* [File structure](#file-structure)
* [Code formatting](#code-formatting)


# File structure

Ovum's file structure is by no means immutable. If a better way of organising files arises, files may well be moved around. This section describes the current structure.

> Organising files in `src/` into subdirectories reduces the risk of having a single gigantic directory with everything.

* `inc/` – Public header files go here. If it's not supposed to be visible to a consumer of the Ovum API, don't put it here.
* `src/` – Everything else, including internal header files.
  - `vm.h` – The only file permitted directly inside the `src/` directory. It forward-declares a variety of commonly-used classes, includes the public API header, defines certain common handles (`ThreadHandle`, `TypeHandle`, `ModuleHandle` etc.) in terms of internal types, and imports the correct OS-specific code (members of `ovum::os`).
  - `debug/` – Debug symbols (and, in the future, debugger features).
  - `ee/` – Execution engine. This includes the `ovum::VM` class, `ovum::Thread`, bytecode parsing and evaluation, and most things that concern execution of code.
  - `gc/` – Garbage collector, including the `ovum::GC` class, `ovum::GCObject` and several related things.
  - `module/` – The module reader, plus related classes.
  - `object/` – The basic object model. Here are the classes `ovum::Type`, `ovum::Method`, `ovum::Field`, `ovum::Property` and other types that define the object model. Helper functions for working with `Value` are here too.
  - `os/` – OS-specific code, organised by operating system.
    + `_template/` – Template files for new operating systems. This directory *must* be kept up to date!
    + `windows/` – Implementation of `ovum::os` members for Microsoft Windows.
    + Each OS has its own header file directly inside `src/os/`, with a name matching the implementing directory. So to get the members of `src/os/windows/`, include `src/os/windows.h`.
  - `res/` – Resources and resource management. This folder is *primarily* used for things internal to Ovum.
  - `threading/` – Various multi-threading primitives, such as mutexes, semaphores, spinlocks and thread-local storage features. Note that `ovum::Thread` is in `src/ee/`, not here.
  - `unicode/` – Unicode-related functionality. UTF-recoders are necessary for internal things, but the general `unicode.h` header file will be moved to aves.
  - `util/` – Assorted utility things that don't seem to fit into any of the other folders. These items are a prime candidate for moving elsewhere.


# Code formatting

## Maximum line length

**Soft limit: 120.** Endeavour to keep lines below 80 characters.

> A hard limit of 80 characters is seen as too restrictive in an age of significant wide-screen monitor adoption. But many developers still wish to view multiple code windows side-by-side, so keep your lines short.

Long string literals may cause the line to exceed 120 characters; that's usually fine. (Consider putting such strings outside the method body, so as to avoid clutter.)

Generated code is exempt from this rule.

## Indentation

**Indent with tabs, align with spaces.** Recommended tab width is 2 or 4 spaces.

> Indenting with tabs lets everyone pick their preferred indentation size; aligning with spaces ensures code looks consistent for everyone (assuming a monospace font is in use).

*(Code examples in this document are indented with 2 spaces because most browsers default to 8 spaces for a tab, which is rather unsightly. Feel free to consider this an argument in favour of spaces.)*

## Alignment

**Align sparingly.** If you must align, do so with spaces, never tabs.

> Alignment is useful in situations where the data is highly unlikely to change much over time, and is most frequently used to align values in enum declarations. Downsides include added maintenance cost and unnecessarily large diffs.

There are some situations where it's *never* acceptable to align:

* Values in local variable declarations;
* Values in constant initializers;
* Default parameter values (if the parameter list is broken up over multiple lines);
* Comments at the end of lines (put the comment above instead); and
* Across different levels of indentation (tabs can vary in size).

In addition, alignment is done *only* on consecutive lines. If there is anything inbetween, even a blank line, do not use alignment. Be aware that overuse of alignment can result in needlessly large diffs. If in doubt, don't do it.

## Blank lines

**Blank lines are *blank*.** You may not leave trailing white space on the line. Configure your editor to strip it.

> If trailing white space on blank lines were permitted but not required, inconsistencies would arise. If required, it would be a nightmare to maintain. Since all widely-used editors can be configured to strip trailing white space on save, this is the least painful alternative.

Blank lines should be used sparingly, but are *required* under some circumstances:

* Between definitions of types, methods, and operator overloads.
* Between *public and protected* fields. Private fields may (but are not required to) be grouped together.
* To separate logical sections in a method. But beware: if your method has a great number of sections, it probably does too many things.

Do not use multiple consecutive blank lines.

**All files end with exactly one newline character,** which your editor may display as a blank line.

## Trailing spaces

**No.** Configure your editor to strip them. No exceptions.

## TODO: More code formatting rules
