//
// The original Sargon functions
//

#ifndef ZARGON_FUNCTIONS_H_INCLUDED
#define ZARGON_FUNCTIONS_H_INCLUDED

//
// Originally all functions were void functions with no parameters,
//  but this changes as the plan to progressively convert them to
//  idiomatic C is realised
//
void INITBD();
inline uint8_t PATH( int8_t dir );
void MPIECE();
void ENPSNT();
inline void ADJPTR();
void CASTLE();
void ADMOVE();
void GENMOV();
inline bool INCHK( uint8_t color );
bool ATTACK();
inline void ATKSAV( uint8_t scan_count, int8_t dir );
inline bool PNCK( uint16_t pin_count, int8_t attack_direction );
void PINFND();
void XCHNG();
inline uint8_t NEXTAD( uint8_t& count, uint8_t* &p );
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
uint16_t BITASN( uint8_t idx );
void ASNTBI();
bool VALMOV();
void ROYALT();
void DIVIDE();
void MLTPLY();
void EXECMV();

// PATH() is the most important function from a performance perspective
// Defining uint8_t PATH( int8_t dir ) as a macro instead of a function
// like this doesn't improve performance, which demonstrates that the
// inline keyword is doing its job
// #define PATH(dir) (m.BOARDA[m.M2+=(dir)] == ((uint8_t)-1) ? 3 : \
//             (                                                   \
//                 0 == (m.T2 = 7&(m.P2 = m.BOARDA[m.M2])) ? 0 :   \
//                     (                                           \
//                         (m.P2 ^ m.P1)&0x80 ? 1 : 2              \
//                     )                                           \
//             )                                                   \
//         )

#endif // ZARGON_FUNCTIONS_H_INCLUDED
