#ifndef __ZERO_KEYSYM_H__
#define __ZERO_KEYSYM_H__

#define KEYSYM_VOID             0x0fffffff

/* TTY function keys; map to ASCII for programming convenience */
#define _keysymttytoascii(ks)   ((ks) & 0x7f)
#define KEYSYM_BACKSPACE        0x0fffff08
#define KEYSYM_TAB              0x0fffff09
#define KEYSYM_LINEFEED         0x0fffff0a
#define KEYSYM_CLEAR            0x0fffff0b
#define KEYSYM_RETURN           0x0fffff0d
#define KEYSYM_PAUSE            0x0fffff13
#define KEYSYM_SCROLL_LOCK      0x0fffff14
#define KEYSYM_SYS_REQ          0x0fffff15
#define KEYSYM_ESCAPE           0x0fffff1b
#define KEYSYM_DELETE           0x0fffffff

/* cursor control and motion */
#define KEYSYM_HOME             0x0fffff50
#define KEYSYM_LEFT             0x0fffff51
#define KEYSYM_UP               0x0fffff52
#define KEYSYM_RIGHT            0x0fffff53
#define KEYSYM_DOWN             0x0fffff54
#define KEYSYM_PRIOR            0x0fffff55
#define KEYSYM_PAGE_UP          KEYSYM_PRIOR
#define KEYSYM_NEXT             0x0fffff56
#define KEYSYM_PAGE_DOWN        KEYSYM_NEXT
#define KEYSYM_END              0x0fffff57
#define KEYSYM_BEGIN            0x0fffff58

/* miscellaneous functions */
#define KEYSYM_SELECT           0x0fffff60
#define KEYSYM_PRINT            0x0fffff61
#define KEYSYM_EXECUTE          0x0fffff62
#define KEYSYM_INSERT           0x0fffff63
#define KEYSYM_UNDO             0x0fffff65
#define KEYSYM_REDO             0x0fffff66
#define KEYSYM_MENU             0x0fffff67
#define KEYSYM_FIND             0x0fffff68
#define KEYSYM_CANCEL           0x0fffff69
#define KEYSYM_HELP             0x0fffff6a
#define KEYSYM_BREAK            0x0fffff6b
#define KEYSYM_MODE_SWITCH      0x0fffff7e
#define KEYSYM_SCRIPT_SWITCH    KEYSYM_MODE_SWITCH
#define KEYSYM_NUM_LOCK         0x0fffff7f

/* keypad functions; keypad numbers map to ASCII for programming convenience */
#define KEYSYM_KP_SPACE         0x0fffff80
#define KEYSYM_KP_TAB           0x0fffff89
#define KEYSYM_KP_ENTER         0x0fffff8d
#define KEYSYM_KP_F1            0x0fffff91
#define KEYSYM_KP_F2            0x0fffff92
#define KEYSYM_KP_F3            0x0fffff93
#define KEYSYM_KP_F4            0x0fffff94
#define KEYSYM_KP_HOME          0x0fffff95
#define KEYSYM_KP_LEFT          0x0fffff96
#define KEYSYM_KP_UP            0x0fffff97
#define KEYSYM_KP_RIGHT         0x0fffff98
#define KEYSYM_KP_DOWN          0x0fffff99
#define KEYSYM_KP_PRIOR         0x0fffff9a
#define KEYSYM_KP_PAGE_UP       KEYSYM_KP_PRIOR
#define KEYSYM_KP_NEXT          0x0fffff9b
#define KEYSYM_KP_PAGE_DOWN     KEYSYM_KP_NEXT
#define KEYSYM_KP_END           0x0fffff9c
#define KEYSYM_KP_BEGIN         0x0fffff9d
#define KEYSYM_KP_INSERT        0x0fffff9e
#define KEYSYM_KP_DELETE        0x0fffff9f
#define KEYSYM_KP_EQUAL         0x0fffffbd
#define KEYSYM_KP_MULTIPLY      0x0fffffaa
#define KEYSYM_KP_ADD           0x0fffffab
#define KEYSYM_KP_SEPARATOR     0x0fffffac
#define KEYSYM_KP_SUBTRACT      0x0fffffad
#define KEYSYM_KP_DECIMAL       0x0fffffae
#define KEYSYM_KP_DIVIDE        0x0fffffaf
/* keypad numbers */
#define _keysympadtoascii(ks)   ((ks) & 0x3f)
#define _keysympadtodigit(ks)   ((ks) - KEYSYM_KP_0)
#define KEYSYM_KP_0             0x0fffffb0
#define KEYSYM_KP_1             0x0fffffb1
#define KEYSYM_KP_2             0x0fffffb2
#define KEYSYM_KP_3             0x0fffffb3
#define KEYSYM_KP_4             0x0fffffb4
#define KEYSYM_KP_5             0x0fffffb5
#define KEYSYM_KP_6             0x0fffffb6
#define KEYSYM_KP_7             0x0fffffb7
#define KEYSYM_KP_8             0x0fffffb8
#define KEYSYM_KP_9             0x0fffffb9

/* function keys */
#define _keysymftoid(ks)        ((ks) - KEYSYM_F1)
#define KEYSYM_F1               0x0fffffbe
#define KEYSYM_F2               0x0fffffbf
#define KEYSYM_F3               0x0fffffc0
#define KEYSYM_F4               0x0fffffc1
#define KEYSYM_F5               0x0fffffc2
#define KEYSYM_F6               0x0fffffc3
#define KEYSYM_F7               0x0fffffc4
#define KEYSYM_F8               0x0fffffc5
#define KEYSYM_F9               0x0fffffc6
#define KEYSYM_F10              0x0fffffc7
#define KEYSYM_F11              0x0fffffc8
#define KEYSYM_L1               0x0fffffc8
#define KEYSYM_F12              0x0fffffc9
#define KEYSYM_L2               0x0fffffc9
#define KEYSYM_F13              0x0fffffca
#define KEYSYM_L3               0x0fffffca
#define KEYSYM_F14              0x0fffffcb
#define KEYSYM_L4               0x0fffffcb
#define KEYSYM_F15              0x0fffffcc
#define KEYSYM_L5               0x0fffffcc
#define KEYSYM_F16              0x0fffffcd
#define KEYSYM_L6               0x0fffffcd
#define KEYSYM_F17              0x0fffffce
#define KEYSYM_L7               0x0fffffce
#define KEYSYM_F18              0x0fffffcf
#define KEYSYM_L8               0x0fffffcf
#define KEYSYM_F19              0x0fffffd0
#define KEYSYM_L9               0x0fffffd0
#define KEYSYM_F20              0x0fffffd1
#define KEYSYM_L10              0x0fffffd1
#define KEYSYM_F21              0x0fffffd2
#define KEYSYM_R1               0x0fffffd2
#define KEYSYM_F22              0x0fffffd3
#define KEYSYM_R2               0x0fffffd3
#define KEYSYM_F23              0x0fffffd4
#define KEYSYM_R3               0x0fffffd4
#define KEYSYM_F24              0x0fffffd5
#define KEYSYM_R4               0x0fffffd5
#define KEYSYM_F25              0x0fffffd6
#define KEYSYM_R5               0x0fffffd6
#define KEYSYM_F26              0x0fffffd7
#define KEYSYM_R6               0x0fffffd7
#define KEYSYM_F27              0x0fffffd8
#define KEYSYM_R7               0x0fffffd8
#define KEYSYM_F28              0x0fffffd9
#define KEYSYM_R8               0x0fffffd9
#define KEYSYM_F29              0x0fffffda
#define KEYSYM_R9               0x0fffffda
#define KEYSYM_F30              0x0fffffdb
#define KEYSYM_R10              0x0fffffdb
#define KEYSYM_F31              0x0fffffdc
#define KEYSYM_R11              0x0fffffdc
#define KEYSYM_F32              0x0fffffdd
#define KEYSYM_R12              0x0fffffdd
#define KEYSYM_F33              0x0fffffde
#define KEYSYM_R13              0x0fffffde
#define KEYSYM_F34              0x0fffffdf
#define KEYSYM_R14              0x0fffffdf
#define KEYSYM_F35              0x0fffffe0
#define KEYSYM_R15              0x0fffffe0

/* modifier keys */
#define KEYSYM_SHIFT_LEFT       0x0fffffe1
#define KEYSYM_SHIFT_RIGHT      0x0fffffe2
#define KEYSYM_CONTROL_LEFT     0x0fffffe3
#define KEYSYM_CONTROL_RIGHT    0x0fffffe4
#define KEYSYM_CAPS_LOCK        0x0fffffe5
#define KEYSYM_SHIFT_LOCK       0x0fffffe6

#define KEYSYM_META_LEFT        0x0fffffe7
#define KEYSYM_META_RIGHT       0x0fffffe8
#define KEYSYM_ALT_LEFT         0x0fffffe9
#define KEYSYM_ALT_RIGHT        0x0fffffea
#define KEYSYM_SUPER_LEFT       0x0fffffeb
#define KEYSYM_SUPER_RIGHT      0x0fffffec
#define KEYSYM_HYPER_LEFT       0x0fffffed
#define KEYSYM_HYPER_RIGHT      0x0fffffee

/* Latin 1, i.e. ISO 8859-1 -> Unicode U+0020..U+00ff */
#define KEYSYM_SPACE            0x00000020
#define KEYSYM_EXCLAMATION      0x00000021
#define KEYSYM_DOUBLE_QUOTE     0x00000022
#define KEYSYM_NUMBER_SIGN      0x00000023
#define KEYSYM_DOLLAR           0x00000024
#define KEYSYM_PERCENT          0x00000025
#define KEYSYM_AMPERSAND        0x00000026
#define KEYSYM_APOSTROPHE       0x00000027
#define KEYSYM_QUOTE_RIGHT      KEYSYM_APOSTROPHE
#define KEYSYM_PAREN_LEFT       0x00000028
#define KEYSYM_PAREN_RIGHT      0x00000029
#define KEYSYM_ASTERISK         0x0000002a
#define KEYSYM_PLUS             0x0000002b
#define KEYSYM_COMMA            0x0000002c
#define KEYSYM_MINUS            0x0000002d
#define KEYSYM_PERIOD           0x0000002e
#define KEYSYM_SLASH            0x0000002f
/* numbers */
#define KEYSYM_0                0x00000030
#define KEYSYM_1                0x00000031
#define KEYSYM_2                0x00000032
#define KEYSYM_3                0x00000033
#define KEYSYM_4                0x00000034
#define KEYSYM_5                0x00000035
#define KEYSYM_6                0x00000036
#define KEYSYM_7                0x00000037
#define KEYSYM_8                0x00000038
#define KEYSYM_9                0x00000039
/* */
#define KEYSYM_COLON            0x0000003a
#define KEYSYM_SEMICOLON        0x0000003b
#define KEYSYM_LESS             0x0000003c
#define KEYSYM_EQUAL            0x0000003d
#define KEYSYM_GREATER          0x0000003e
#define KEYSYM_QUESTION         0x0000003f
#define KEYSYM_AT               0x00000040
/* upper-case alphabetics */
#define KEYSYM_A                0x00000041
#define KEYSYM_B                0x00000042
#define KEYSYM_C                0x00000043
#define KEYSYM_D                0x00000044
#define KEYSYM_E                0x00000045
#define KEYSYM_F                0x00000046
#define KEYSYM_G                0x00000047
#define KEYSYM_H                0x00000048
#define KEYSYM_I                0x00000049
#define KEYSYM_J                0x0000004a
#define KEYSYM_K                0x0000004b
#define KEYSYM_L                0x0000004c
#define KEYSYM_M                0x0000004d
#define KEYSYM_N                0x0000004e
#define KEYSYM_O                0x0000004f
#define KEYSYM_P                0x00000050
#define KEYSYM_Q                0x00000051
#define KEYSYM_R                0x00000052
#define KEYSYM_S                0x00000053
#define KEYSYM_T                0x00000054
#define KEYSYM_U                0x00000055
#define KEYSYM_V                0x00000056
#define KEYSYM_W                0x00000057
#define KEYSYM_X                0x00000058
#define KEYSYM_Y                0x00000059
#define KEYSYM_Z                0x0000005a
/* */
#define KEYSYM_BRACKET_LEFT     0x0000005b
#define KEYSYM_BACKSLASH        0x0000005c
#define KEYSYM_BRACKET_RIGHT    0x0000005d
#define KEYSYM_ASCII_CIRCUM     0x0000005e
#define KEYSYM_UNDERSCORE       0x0000005f
#define KEYSYM_GRAVE            0x00000060
#define KEYSYM_QUOTE_LEFT       KEYSYM_GRAVE
/* lower-case alphabetics */
#define KEYSYM_a                0x00000061
#define KEYSYM_b                0x00000062
#define KEYSYM_c                0x00000063
#define KEYSYM_d                0x00000064
#define KEYSYM_e                0x00000065
#define KEYSYM_f                0x00000066
#define KEYSYM_g                0x00000067
#define KEYSYM_h                0x00000068
#define KEYSYM_i                0x00000069
#define KEYSYM_j                0x0000006a
#define KEYSYM_k                0x0000006b
#define KEYSYM_l                0x0000006c
#define KEYSYM_m                0x0000006d
#define KEYSYM_n                0x0000006e
#define KEYSYM_o                0x0000006f
#define KEYSYM_p                0x00000070
#define KEYSYM_q                0x00000071
#define KEYSYM_r                0x00000072
#define KEYSYM_s                0x00000073
#define KEYSYM_t                0x00000074
#define KEYSYM_u                0x00000075
#define KEYSYM_v                0x00000076
#define KEYSYM_w                0x00000077
#define KEYSYM_x                0x00000078
#define KEYSYM_y                0x00000079
#define KEYSYM_z                0x0000007a
/* */
#define KEYSYM_BRACE_LEFT       0x0000007b
#define KEYSYM_BAR              0x0000007c
#define KEYSYM_BRACE_RIGHT      0x0000007d
#define KEYSYM_ASCII_TILDE      0x0000007e
#define KEYSYM_NOBREAK_SPACE    0x000000a0
#define KEYSYM_EXCLAMATION_DOWN 0x000000a1
#define KEYSYM_CENT             0x000000a2
#define KEYSYM_STERLING         0x000000a3
#define KEYSYM_CURRENCY         0x000000a4
#define KEYSYM_YEN              0x000000a5
#define KEYSYM_BROKEN_BAR       0x000000a6
#define KEYSYM_SECTION          0x000000a7
#define KEYSYM_DIAERESIS        0x000000a8
#define KEYSYM_COPYRIGHT        0x000000a9
#define KEYSYM_ORDFEMININE      0x000000aa
#define KEYSYM_GUILLEMOT_LEFT   0x000000ab
#define KEYSYM_NOT_SIGN         0x000000ac
#define KEYSYM_SOFT_HYPHEN      0x000000ad
#define KEYSYM_REGISTERED_SIGN  0x000000ae
#define KEYSYM_MACRON           0x000000af
#define KEYSYM_DEGREE           0x000000b0
#define KEYSYM_PLUS_MINUS       0x000000b1
#define KEYSYM_TWO_SUPERIOR     0x000000b2
#define KEYSYM_THREE_SUPERIOR   0x000000b3
#define KEYSYM_ACUTE            0x000000b4
#define KEYSYM_MICRO_SIGN       0x000000b5
#define KEYSYM_PILCROW_SIGN     0x000000b6
#define KEYSYM_PARAGRAPH        KEYSYM_PILCROW_SIGN
#define KEYSYM_PERIOD_CENTERED  0x000000b7
#define KEYSYM_CEDILLA          0x000000b8
#define KEYSYM_ONE_SUPERIOR     0x000000b9
#define KEYSYM_ORDMASCULINE     0x000000ba
#define KEYSYM_GUILLEMOT_RIGHT  0x000000bb
#define KEYSYM_ONE_QUARTER      0x000000bc
#define KEYSYM_ONE_HALF         0x000000bd
#define KEYSYM_THREE_QUARTERS   0x000000be
#define KEYSYM_QUESTION_DOWN    0x000000bf
#define KEYSYM_A_GRAVE          0x000000c0
#define KEYSYM_A_ACUTE          0x000000c1
#define KEYSYM_A_CIRCUMFLEX     0x000000c2
#define KEYSYM_A_TILDE          0x000000c3
#define KEYSYM_A_DIAERESIS      0x000000c4
#define KEYSYM_A_RING_ABOVE     0x000000c5
#define KEYSYM_AE               0x000000c6
#define KEYSYM_C_CEDILLA        0x000000c7
#define KEYSYM_E_GRAVE          0x000000c8
#define KEYSYM_E_ACUTE          0x000000c9
#define KEYSYM_E_CIRCUMFLEX     0x000000ca
#define KEYSYM_E_DIAERESIS      0x000000cb
#define KEYSYM_I_GRAVE          0x000000cc
#define KEYSYM_I_ACUTE          0x000000cd
#define KEYSYM_I_CIRCUMFLEX     0x000000ce
#define KEYSYM_I_DIAERESIS      0x000000cf
#define KEYSYM_ETH              0x000000d0
#define KEYSYM_N_TILDE          0x000000d1
#define KEYSYM_O_GRAVE          0x000000d2
#define KEYSYM_O_ACUTE          0x000000d3
#define KEYSYM_O_CIRCUMFLEX     0x000000d4
#define KEYSYM_O_TILDE          0x000000d5
#define KEYSYM_O_DIAERESIS      0x000000d6
#define KEYSYM_MULTIPLY         0x000000d7
#define KEYSYM_O_STROKE         0x000000d8
#define KEYSYM_O_OBLIQUE        KEYSYM_O_STROKE
#define KEYSYM_U_GRAVE          0x000000d9
#define KEYSYM_U_ACUTE          0x000000da
#define KEYSYM_U_CIRCUMFLEX     0x000000db
#define KEYSYM_U_DIAERESIS      0x000000dc
#define KEYSYM_Y_ACUTE          0x000000dd
#define KEYSYM_THORN            0x000000de
#define KEYSYM_s_SHARP          0x000000df
#define KEYSYM_a_GRAVE          0x000000e0
#define KEYSYM_a_ACUTE          0x000000e1
#define KEYSYM_a_CIRCUMFLEX     0x000000e2
#define KEYSYM_a_TILDE          0x000000e3
#define KEYSYM_a_DIAERESIS      0x000000e4
#define KEYSYM_a_RING_ABOVE     0x000000e5
#define KEYSYM_ae               0x000000e6
#define KEYSYM_c_CEDILLA        0x000000e7
#define KEYSYM_e_GRAVE          0x000000e8
#define KEYSYM_e_ACUTE          0x000000e9
#define KEYSYM_e_CIRCUMFLEX     0x000000ea
#define KEYSYM_e_DIAERESIS      0x000000eb
#define KEYSYM_i_GRAVE          0x000000ec
#define KEYSYM_i_ACUTE          0x000000ed
#define KEYSYM_i_CIRCUMFLEX     0x000000ee
#define KEYSYM_i_DIAERESIS      0x000000ef
#define KEYSYM_eth              0x000000f0
#define KEYSYM_n_TILDE          0x000000f1
#define KEYSYM_o_GRAVE          0x000000f2
#define KEYSYM_o_ACUTE          0x000000f3
#define KEYSYM_o_CIRCUMFLEX     0x000000f4
#define KEYSYM_o_TILDE          0x000000f5
#define KEYSYM_o_DIAERESIS      0x000000f6
#define KEYSYM_DIVISION         0x000000f7
#define KEYSYM_o_STROKE         0x000000f8
#define KEYSYM_o_OBLIQUE        KEYSYM_o_STROKE
#define KEYSYM_u_GRAVE          0x000000f9
#define KEYSYM_u_ACUTE          0x000000fa
#define KEYSYM_u_CIRCUMFLEX     0x000000fb
#define KEYSYM_u_DIAERESIS      0x000000fc
#define KEYSYM_y_ACUTE          0x000000fd
#define KEYSYM_thorn            0x000000fe
#define KEYSYM_y_DIAERESIS      0x000000ff

#endif /* __ZERO_KEYSYM_H__ */

