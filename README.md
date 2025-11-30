What is this?
=============

The book "Sargon, a computer chess program", by Dan and Kathe Spracklen
published by Hayden in 1978 presents the source code to the classic
early chess program Sargon in Z80 assembly language. This is my *SECOND* project
to bring the code presented in the book back to life in the modern era.

I haven't finished this second project yet, so for now I will refer you
to [my first Sargon project](https://github.com/billforsternz/retro-sargon). Having
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
it. It seems to me if I have a C version, even a slightly weird C version (check out
the zargon.cpp excerpt below - it is a bit weird) I will have a good chance of
understanding it and making it understandable, by progressively converting it to
idiomatic, well commented C, one function at a time.
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
- Maybe with all other goals met it might be interesting to try and make a stronger
version of the engine.

This is an extract from zargon.cpp, it includes numbered lines from the original Z80 code
on the right as comments. I hand converted it to statements that C can deal with given
a bunch of suitable macros. Please feel free to dig into the code for more information.
The [Commit history](https://github.com/billforsternz/zargon/commits/main/) might also
be of interest.

<pre><code><br>
//***********************************************************              //0821: ;***********************************************************
// GENERATE MOVE ROUTINE                                                   //0822: ; GENERATE MOVE ROUTINE
//***********************************************************              //0823: ;***********************************************************
// FUNCTION:  --  To generate the move set for all of the                  //0824: ; FUNCTION:  --  To generate the move set for all of the
//                pieces of a given color.                                 //0825: ;                pieces of a given color.
//                                                                         //0826: ;
// CALLED BY: --  FNDMOV                                                   //0827: ; CALLED BY: --  FNDMOV
//                                                                         //0828: ;
// CALLS:     --  MPIECE                                                   //0829: ; CALLS:     --  MPIECE
//                INCHK                                                    //0830: ;                INCHK
//                                                                         //0831: ;
// ARGUMENTS: --  None                                                     //0832: ; ARGUMENTS: --  None
//***********************************************************              //0833: ;***********************************************************
void GENMOV() {
        callback_zargon_bridge(CB_GENMOV);
        CALLu   (INCHK);                //  Test for King in check         //0834: GENMOV: CALL    INCHK           ; Test for King in check
        LD      (val(CKFLG),a);         //  Save attack count as flag      //0835:         LD      (CKFLG),a       ; Save attack count as flag
        LD      (de,v16(MLNXT));        //  Addr of next avail list space  //0836:         LD      de,(MLNXT)      ; Addr of next avail list space
        LD      (hl,v16(MLPTRI));       //  Ply list pointer index         //0837:         LD      hl,(MLPTRI)     ; Ply list pointer index
        INC16   (hl);                   //  Increment to next ply          //0838:         INC     hl              ; Increment to next ply
        INC16   (hl);                                                      //0839:         INC     hl
        LD      (ptr(hl),e);            //  Save move list pointer         //0840:         LD      (hl),e          ; Save move list pointer
        INC16   (hl);                                                      //0841:         INC     hl
        LD      (ptr(hl),d);                                               //0842:         LD      (hl),d
        INC16   (hl);                                                      //0843:         INC     hl
        LD      (v16(MLPTRI),hl);       //  Save new index                 //0844:         LD      (MLPTRI),hl     ; Save new index
        LD      (v16(MLLST),hl);        //  Last pointer for chain init.   //0845:         LD      (MLLST),hl      ; Last pointer for chain init.
        LD      (a,21);                 //  First position on board        //0846:         LD      a,21            ; First position on board
GM5:    LD      (val(M1),a);            //  Save as index                  //0847: GM5:    LD      (M1),a          ; Save as index
        LD      (ix,v16(M1));           //  Load board index               //0848:         LD      ix,(M1)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents           //0849:         LD      a,(ix+BOARD)    ; Fetch board contents
        AND     (a);                    //  Is it empty ?                  //0850:         AND     a               ; Is it empty ?
        JR      (Z,GM10);               //  Yes - Jump                     //0851:         JR      Z,GM10          ; Yes - Jump
        CP      (-1);                   //  Is it a border square ?        //0852:         CP      -1              ; Is it a border square ?
        JR      (Z,GM10);               //  Yes - Jump                     //0853:         JR      Z,GM10          ; Yes - Jump
        LD      (val(P1),a);            //  Save piece                     //0854:         LD      (P1),a          ; Save piece
        LD      (hl,addr(COLOR));       //  Address of color of piece      //0855:         LD      hl,COLOR        ; Address of color of piece
        XOR     (ptr(hl));              //  Test color of piece            //0856:         XOR     (hl)            ; Test color of piece
        BIT     (7,a);                  //  Match ?                        //0857:         BIT     7,a             ; Match ?
        CALL    (Z,MPIECE);             //  Yes - call Move Piece          //0858:         CALL    Z,MPIECE        ; Yes - call Move Piece
GM10:   LD      (a,val(M1));            //  Fetch current board position   //0859: GM10:   LD      a,(M1)          ; Fetch current board position
        INC     (a);                    //  Incr to next board position    //0860:         INC     a               ; Incr to next board position
        CP      (99);                   //  End of board array ?           //0861:         CP      99              ; End of board array ?
        JP      (NZ,GM5);               //  No - Jump                      //0862:         JP      NZ,GM5          ; No - Jump
        RETu;                           //  Return                         //0863:         RET                     ; Return
}                                                                          //0864:
</code></pre>

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
