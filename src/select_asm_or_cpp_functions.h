//
// Select switchable function implementations
//

#ifndef SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED
#define SELECT_ASM_OR_CPP_FUNCTIONS_H_INCLUDED

#define PATH   path_c
#define ATTACK attack_c
#define ATKSAV atksav_c
#define PNCK   pnck_c

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
