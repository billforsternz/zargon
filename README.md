What is this?
=============

The book "Sargon, a computer chess program", by Dan and Kathe Spracklen
published by Hayden in 1978 presents the source code to the classic
early chess program Sargon in Z80 assembly language. This is my *SECOND* project
to bring the code presented in the book back to life in the modern era.

I haven't finished this second project yet, so for now I will refer you
to (my first Sargon project)[https://github.com/billforsternz/retro-sargon]. Having
said that....

Preliminary Information about Zargon
====================================

For my first Sargon 1978 project I translated the original Sargon Z80 assembly to x86 assembly.
This was pretty successful, and ultimately produced a fun, fully working Sargon 1978 UCI engine
for Windows.

For this project I decided I could go further with a different approach. This time
I translated the Sargon Z80 assembly to C (actually C++, but it's mostly the C
subset of C++). To try and avoid confusion and to cleanly separate this from my
first Sargon project (let's call that one Sargon-x86) this 100% C/C++ Sargon project
is called Zargon instead.

There are several motivations behind Zargon;
- 32 bit x86 assembly is a dead end. Even 64 bit x64 bit is increasingly looking like
a second class citizen in an ARM dominated world. By using C I can make Sargon 1978
available to anybody who's interested on any computer.
- I remain fascinated by the Spracklen's code, but I still don't *really* understand
it. It seems to me if I have a C version, even a stlightly weird C version (check out
zargon.cpp - it is a bit weird) I will have a good chance of understanding it and
making it understandable, by progressively converting it to idiomatic, well commented
C, one function at a time.
- Paradoxically it's faster. Although assembler -> assembler sounds like the route to
ultimate performance, the x86 assembler from my first project was undeniably a bit
clunky. The Intel processors it runs on have millions of transistors, and the code
I asked them to run was kind of like getting them to run like a speeded up 1970s
CPU with thousands of transistors. A C version plus a modern compiler removes the
handcuffs and lets Sargon fly. Initially when I got Zargon to work (that's right,
it is working) it was simply a line by line translation, and it was slower than
Sargon-x86, as I expected. But not that much slower. And rewriting a few functions
in the hot path (PATH, ATTACK, ATKSAV, PNCK) has already got Zargon running twice
as fast as Sargon-x86.
- Maybe it can be *much* faster. I don't know much about modern engine programming.
But I do know that they use transposition tables and that this technique alone can
speed them by orders of magnitude. Ultimately I hope to bring tranposition tables
to Zargon and generate the exact same moves as Sargon-x86 millions of times faster
than the original rather than just tens of thousands of times faster. It's possible
I'm a fool.

At the moment I have Zargon running nicely, twice as fast as the original Sargon-x86.
I only have it running in the context of the test harness from the original project
(same test harness code, same test results). I haven't bothered to create a Zargon
UCI engine just yet, although it's a routine job I'm sure I could do in an hour or
so when I feel the need.

Acknowledgment of Original Authorship
=====================================

(This is the same concluding paragraph I have in my original Sargon project)

I acknowledge the ownership of legendary programmers Dan and Kathe
Spracklen. When I started on my first Sargon project I added that if they contact me
in respect of this project, I will honour their wishes, whatever they
may be. Since then though I have put a huge amount of effort into
polishing this into a model project. I've opened up a window into the
program, and brought it back to life. I think I would have a great deal
of difficulty deleting this project now, if I was asked to do that.
Hopefully a purely hypothetical question. There is clearly no commercial
motive of incentive for anyone in this project. As I wrote on Twitter "I
am porting a 70s chess program written in Z80 assembly to run on modern
machines with a standard chess engine interface. Never has a project
been more guaranteed not to generate a single cent of revenue."
