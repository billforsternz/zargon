REM
REM Inputs sargon-8080-and-x86.asm (prepared manually from sargon4.asm)
REM Outputs sargon-z80.asm and sargon-x86.asm
REM
convert-8080-to-z80-or-x86.exe -generate_z80_only sargon-8080-and-x86.asm sargon-z80.asm temp.h temp.txt
convert-8080-to-z80-or-x86.exe -generate_x86 -relax sargon-8080-and-x86.asm sargon-x86.asm temp.h temp.txt
del temp.h
del temp.txt
REM
REM Next manually merge the significant difference in sargon-x86.asm
REM into src/sargon-x86.asm which has had many manual changes (eg
REM CALLBACK statements) since it was automatically generated.
REM

