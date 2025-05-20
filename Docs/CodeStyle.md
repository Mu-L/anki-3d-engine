This document describes the style of code in AnKi 3D Engine. The code is written primarily in C++ with some HLSL and python as well.

Table of contents
=================

* [Comments in C++ & HLSL](#Comments%20in%20C++%20&%20HLSL)
* [Comments in python](#Comments%20in%20python)
* [Naming conventions in C++ & HLSL](#Naming%20conventions%20in%20C++%20&%20HLSL)
* [Naming conventions in python](#Naming%20conventions%20in%20python)
* [Naming conventions for files and directories](#Naming%20conventions%20for%20files%20and%20directories)
* [C++ rules](#C++%20rules)
* [HLSL rules](#HLSL%20rules)
* [Code formatting in C++ & HLSL](#Code%20formatting%20in%20C++%20&%20HLSL)
* [Code formatting in python](#Code%20formatting%20in%20python)

Comments in C++ & HLSL
======================

All comments are C++ like (double slash):

	// Some comment

And for doxygen comments in C++ only:

	/// @brief blah blah
	/// @param blah blah

Comments in python
==================

Whatever the PEP 8 guide proposes.

Naming conventions in C++ & HLSL
================================

**Variables, functions and methods** are lower camelCase.

	functionDoSomething(someVariableA, someVariableB);

All **types** are PascalCase and that includes classes, structs, typedefs and enums.

	enum MyEnum {...};
	class MyClass {...};
	using MyType = int;

All **constexpr expressions** start with a `k` and that includes enumerants.

	constexpr int kMyConstant = ...;

	enum MyEnum
	{
		kEnumerantA
	};

All **macros and defines** should be SCREAMING_SNAKE_CASE and they should have an `ANKI_` prefix.

	#define ANKI_ASSERT(expr)

	#define ANKI_MAX_SOMETHING ...

All **template typedefs** should start with `T`. The rule doesn't apply in constant template arguments.

	template<typename TSomeType, typename TSomeOtherType, U32 kSomeConst>

All **function and method names** should form a sentence with at least one verb.

	doSomething(...);
	getSomething();
	calculateSomething(...);

**Using some hungarian notations**.

- Constants start with `k` as mentioned above.
- All member variables have the `m_` prefix. That applies to classes and structs.
- All global variables have the `g_` prefix.

Same rules apply to HLSL.

**Variables that act as a measure for quantity** should have the `count` suffix. Not `num` or `numberOf` or similar.

	int appleCount = ...; // YES
	int appleNum = ...;   // NO
	int numApples = ...;  // NO

Naming conventions in python
============================

Whatever the PEP 8 guide proposes.

Naming conventions for files and directories
============================================

**Filenames and directories should be PascalCase**. The extensions of the files are lowercase.

	Source/ThreadHive.h

	Shaders/MyShaderProgram.ankiprog

	Some/Path/Script.py

Header files that **contain inline methods/functions** (for template classes for example) should have the `.inl.h` extension.

Header files that **contain definitions** (are included multiple types with different permutations) should have the `.def.h` extension.

C++ rules
=========

**Always use strongly typed enums**. If you need to relax the rules use the `ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS` macro.

	enum class MyEnum : U32 {...};
	ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(MyEnum);

**Never use `typedef`** to define types. Use `using` instead.

	using U32 = uint32_t;
	using Func = void(*)(int, int);

**Never use C-style casting**. Use static_cast, reinterpret_cast or const_cast. For for type conversions of fudmental types use constructor-like cast.


	Derived* d = (Derived*)base;              // NO
	Derived* d = static_cast<Derived*>(base); // YES!!

	uint i = uint(1.1f); // uint is fudmental type and the expression does a type conversion, in that case use constructor-like cast

**Always use `constexpr`** as much as possible. Also, avoid using defines for constants.

	constexpr uint kSomeConst = ...; // YES!!
	#define SOME_CONST ...           // NO

**Always use `class`** and never use `struct`. Since it's difficult to come up with rules on when to use struct over class, always use class and be done with it.

	class MyTrivialType
	{
	public:
		int m_x;
		int m_y;
	};

**Avoid using `auto`**. It's only allowed in some templates and in iterators. auto makes reviewing code difficult.

	auto i = 10;               // NO
	auto it = list.getBegin(); // ... FINE

**Includes should always have the full file path**. This avoids issues with files of similar name. Also, non-AnKi includes follow AnKi includes.

	#include <AnKi/Util/Thread.h>
	#include <AnKi/Util/WeakArray.h>
	#include <AnKi/Util/Allocator.h>
	#include <cstdio>

**Never use public nested classes**. Only private nested classes are allowed. Nested classes cannot be forward declared. Nested typedefs are fine.

	class MyClass
	{
	public:
		// NO!!!!!!!!!!!
		class Nested {...};

		// OK
		using SomeTypedef = U32;

	private:
		// OK, it's private
		class Nested {...};

		// Also OK, it's private
		class
		{
			...
		} m_myUnamedThing;
	};

Try to **use `explicit`** in constructors for extra safety. Don't use explicit on copy constructors.

AnKi uses **const correctness** throughout and that's mandatory. Always use `const`. The only place where you don't have to is for function and method parameters.

	uint doSomething(uint noNeedForConst)
	{
		const uint bla = noNeedForConst + 10;
		return blah;
	}

**Access types in a class have a specified order**. First `public` then `protected` and last `private`. There are some exceptions where this rule has to break.

	class MyClass
	{
	public:
		...
	protected:
		...
	private:
		...
	};

Always **use AnKi's fudmental types** (U32, I32, F32, Bool etc etc). For integers and if you don't care about the size of the integer you can use the type `U` for unsinged or `I` for signed. Both of them are at least 32bit.

	for(U32 i = 0; i < 10; ++i) {...}
	U32 i = ...;

If you have to overload the operator Bool always use **`explicit operator Bool` operator** overloading. Omitting the `explicit` may lead to bugs.

	explicit operator Bool() const {...}

The use of **references and pointers** is govern by some rules. Always use references except when you transfer or share ownership. Pointers are also allowed for optional data that can be nullptr.

	// ptr is a pointer so you CAN NOT destroy after a call to doSomething is done
	// ref is a reference so you CAN destroy after a call to doSomething is done
	void Foo::doSomething(TypeA* ptr, TypeB& ref)
	{
		// Store the ptr somewhere
		m_someMember = ptr;

		// Won't store the ref
		ref += ...;
	}

Always **use `nullptr`** and never NULL.

	void* ptr = nullptr;

**Never use C style arrays**. Use the `Array<>` template instead.

	Array<MyType, 10> myArray; // YES
	MyType myArray[10];        // NO

**Don't use STL**. AnKi has a replacement for most STL templates. There are some exceptions though.

- Can use `type_traights`.
- Some from `utility` eg std::move.
- Some from `algorithm` eg std::sort.

Always try to **comment parts of the source code**. Self documenting code is never enough.

	void someFunction(...)
	{
		// Now do X
		...

		// Now do Y
		...
	}

Always **use `override` or `final`** on virtual methods and try to use `final` on classes when applicable.

	class Foo final
	{
	public:
		void myVirtual(...) override {...}
	};

**Always use curly braces** on single line expressions.

	// NO!!!!!!!!!!!
	if(something) doSomething();

	// YES!!!!!!!!!!!!!!
	if(something)
	{
		doSomething();
	}

Always place the **condition of a conditional ternary operator** inside parenthesis.

	a =  b  ? 1 : 0;   // NO
	a = (b) ? 1 : 0; // YES

HLSL rules
==========

Same rules as in C++ (when applicable) with one exception: You should **use `struct` instead of `class`** for user defined types.

Code formatting in C++ & HLSL
=============================

clang-format deals with code formatting. To format all the C++ and HLSL files in the source tree type the following in a terminal:

	$ cd /path/to/anki
	$ python ./Tools/FormatSource.py

Code formatting in python
=========================

Whatever the PEP 8 guide proposes.
