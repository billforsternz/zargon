//
// Select switchable function implementations
//

#ifndef SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED
#define SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED

#define PATH   path_c
#define ATTACK attack_c
#define ATKSAV atksav_c
#define PNCK   pnck_c
//bool pnck_c( uint8_t pin_count, int8_t attack_direction );
//void atksav_c(int8_t dir);
bool attack_c();

bool pnck_c( int8_t attack_direction );
void atksav_c(int8_t dir);
//void attack_c();

//
// Sargon function declarations
//
void INITBD();
// void PATH(); // static and inline
void MPIECE();
void ENPSNT();
void ADJPTR();
void CASTLE();
void ADMOVE();
void GENMOV();
void INCHK();
void INCHK1();
bool ATTACK();
//void ATKSAV( int8_t dir );
//bool PNCK();
//void ATTACK();
void ATKSAV( int8_t dir );
bool PNCK();
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


//37300
//#define PATH (m.BOARDA[m.M2+=c] == ((uint8_t)-1) ? 3 :          \
//            (                                                   \
//                0 == (m.T2 = 7&(m.P2 = m.BOARDA[m.M2])) ? 0 :   \
//                    (                                           \
//                        (m.P2 ^ m.P1)&0x80 ? 1 : 2              \
//                    )                                           \
//            )                                                   \
//        )

#endif // SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED
