//
// Select switchable function implementations
//

#ifndef SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED
#define SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED

//
// Sargon function declarations
//
void INITBD();
inline uint8_t PATH( uint8_t dir );
void MPIECE();
void ENPSNT();
void ADJPTR();
void CASTLE();
void ADMOVE();
void GENMOV();
void INCHK();
void INCHK1();
bool ATTACK();
void ATKSAV( uint8_t dir );
bool PNCK( uint16_t pin_count, uint8_t attack_direction );
void PINFND();
void XCHNG();
void NEXTAD();
void POINTS();
void LIMIT();
void MOVE();
void UNMOVE();
void SORTM();
void EVAL();
void FNDMOV();
void ASCEND();
void BOOK();
void CPTRMV();
void BITASN();
void ASNTBI();
void VALMOV();
void ROYALT();
void DIVIDE();
void MLTPLY();
void EXECMV();

// #define PATH(dir) (m.BOARDA[m.M2+=(dir)] == ((uint8_t)-1) ? 3 :          \
//             (                                                   \
//                 0 == (m.T2 = 7&(m.P2 = m.BOARDA[m.M2])) ? 0 :   \
//                     (                                           \
//                         (m.P2 ^ m.P1)&0x80 ? 1 : 2              \
//                     )                                           \
//             )                                                   \
//         )

#endif // SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED
