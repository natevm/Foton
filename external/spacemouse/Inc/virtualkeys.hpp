#ifndef virtualkeys_HPP_INCLUDED_
#define virtualkeys_HPP_INCLUDED_
// virtualkeys.hpp
/*
 * Copyright notice:
 * Copyright (c) 2010-2015 3Dconnexion. All rights reserved. 
 * 
 * This file and source code are an integral part of the "3Dconnexion
 * Software Developer Kit", including all accompanying documentation,
 * and is protected by intellectual property laws. All use of the
 * 3Dconnexion Software Developer Kit is subject to the License
 * Agreement found in the "LicenseAgreementSDK.txt" file.
 * All rights not expressly granted by 3Dconnexion are reserved.
 */
///////////////////////////////////////////////////////////////////////////////////
// History
//
// $Id: virtualkeys.hpp 14192 2017-06-21 16:32:40Z jwick $
//
// 22.10.12 MSB Fix: Number of keys returned for SpacePilot Pro is incorrect (27)
// 19.06.12 MSB Added SM Touch and Generic 2 Button SM
// 09.03.12 MSB Fix VirtualKeyToHid not correctly converting SpaceMousePro buttons >V3DK_3
// 03.02.12 MSB Changed the labels of the "programmable buttons" back to "1" etc
// 22.09.11 MSB Added V3DK_USER above which the users may add their own virtual keys
// 02.08.11 MSB Added pid for Viking
//              Added virtualkey / hid definition for Viking
//              Added member to the tag_VirtualKeys struct for the number of
//              buttons on the device
//              Added methods to retrieve the number of buttons on a device
//              Added methods to map the hid buttons numbers to a consecutive
//              sequence (and back again)
// 11.03.11 MSB Fix incorrect label for V3DK_ROLL_CCW
// 09.03.11 MSB Added methods to return the labels of the keys on the device
// 19.10.10 MSB Moved the standard 3dmouse virtual buttons to the s3dm namespace
// 28.09.10 MSB Added spin and tilt buttons
//              Added structure to convert virtual key number to string identifier
// 04.12.09 MSB Fix spelling mistake 'panzoon'
//

#define _TRACE_VIRTUAL_KEYS 0


#if !defined(numberof)
#define numberof(_x) (sizeof(_x)/sizeof(_x[0]))
#endif

namespace s3dm {

   // This enum comes from trunk/inc/V3DKey.h. They must remain synced.
   enum e3dmouse_virtual_key 
   {
   V3DK_INVALID     = 0,
   V3DK_MENU		= 1,
   V3DK_FIT			= 2,
   V3DK_TOP			= 3,
   V3DK_LEFT		= 4,
   V3DK_RIGHT		= 5,
   V3DK_FRONT		= 6,
   V3DK_BOTTOM		= 7,
   V3DK_BACK		= 8,
   V3DK_ROLL_CW		= 9,
   V3DK_ROLL_CCW	= 10,
   V3DK_ISO1		= 11,
   V3DK_ISO2		= 12,
   V3DK_1			= 13,
   V3DK_2			= 14,
   V3DK_3			= 15,
   V3DK_4			= 16,
   V3DK_5			= 17,
   V3DK_6			= 18,
   V3DK_7			= 19,
   V3DK_8			= 20,
   V3DK_9			= 21,
   V3DK_10			= 22,
   V3DK_ESC			= 23,
   V3DK_ALT			= 24,
   V3DK_SHIFT		= 25,
   V3DK_CTRL		= 26,
   V3DK_ROTATE		= 27,
   V3DK_PANZOOM		= 28,
   V3DK_DOMINANT	= 29,
   V3DK_PLUS		= 30,
   V3DK_MINUS		= 31,
   V3DK_SPIN_CW		= 32,
   V3DK_SPIN_CCW	= 33,
   V3DK_TILT_CW		= 34,
   V3DK_TILT_CCW	= 35,
   V3DK_ENTER		= 36,
   V3DK_DELETE		= 37,
   V3DK_RESERVED0	= 38,
   V3DK_RESERVED1	= 39,
   V3DK_RESERVED2	= 40,
   V3DK_F1			= 41,
   V3DK_F2			= 42,
   V3DK_F3			= 43,
   V3DK_F4			= 44,
   V3DK_F5			= 45,
   V3DK_F6			= 46,
   V3DK_F7			= 47,
   V3DK_F8			= 48,
   V3DK_F9			= 49,
   V3DK_F10			= 50,
   V3DK_F11			= 51,
   V3DK_F12			= 52,
   V3DK_F13			= 53,
   V3DK_F14			= 54,
   V3DK_F15			= 55,
   V3DK_F16			= 56,
   V3DK_F17			= 57,
   V3DK_F18			= 58,
   V3DK_F19			= 59,
   V3DK_F20			= 60,
   V3DK_F21			= 61,
   V3DK_F22			= 62,
   V3DK_F23			= 63,
   V3DK_F24			= 64,
   V3DK_F25			= 65,
   V3DK_F26			= 66,
   V3DK_F27			= 67,
   V3DK_F28			= 68,
   V3DK_F29			= 69,
   V3DK_F30			= 70,
   V3DK_F31			= 71,
   V3DK_F32			= 72,
   V3DK_F33			= 73,
   V3DK_F34			= 74,
   V3DK_F35			= 75,
   V3DK_F36			= 76,
   V3DK_11			= 77,
   V3DK_12			= 78,
   V3DK_13			= 79,
   V3DK_14			= 80,
   V3DK_15			= 81,
   V3DK_16			= 82,
   V3DK_17			= 83,
   V3DK_18			= 84,
   V3DK_19			= 85,
   V3DK_20			= 86,
   V3DK_21			= 87,
   V3DK_22			= 88,
   V3DK_23			= 89,
   V3DK_24			= 90,
   V3DK_25			= 91,
   V3DK_26			= 92,
   V3DK_27			= 93,
   V3DK_28			= 94,
   V3DK_29			= 95,
   V3DK_30			= 96,
   V3DK_31			= 97,
   V3DK_32			= 98,
   V3DK_33			= 99,
   V3DK_34			= 100,
   V3DK_35			= 101,
   V3DK_36			= 102,
   V3DK_VIEW_1		= 103,
   V3DK_VIEW_2		= 104,
   V3DK_VIEW_3		= 105,
   V3DK_VIEW_4		= 106,
   V3DK_VIEW_5		= 107,
   V3DK_VIEW_6		= 108,
   V3DK_VIEW_7		= 109,
   V3DK_VIEW_8		= 110,
   V3DK_VIEW_9		= 111,
   V3DK_VIEW_10		= 112,
   V3DK_VIEW_11		= 113,
   V3DK_VIEW_12		= 114,
   V3DK_VIEW_13		= 115,
   V3DK_VIEW_14		= 116,
   V3DK_VIEW_15		= 117,
   V3DK_VIEW_16		= 118,
   V3DK_VIEW_17		= 119,
   V3DK_VIEW_18		= 120,
   V3DK_VIEW_19		= 121,
   V3DK_VIEW_20		= 122,
   V3DK_VIEW_21		= 123,
   V3DK_VIEW_22		= 124,
   V3DK_VIEW_23		= 125,
   V3DK_VIEW_24		= 126,
   V3DK_VIEW_25		= 127,
   V3DK_VIEW_26		= 128,
   V3DK_VIEW_27		= 129,
   V3DK_VIEW_28		= 130,
   V3DK_VIEW_29		= 131,
   V3DK_VIEW_30		= 132,
   V3DK_VIEW_31		= 133,
   V3DK_VIEW_32		= 134,
   V3DK_VIEW_33		= 135,
   V3DK_VIEW_34		= 136,
   V3DK_VIEW_35		= 137,
   V3DK_VIEW_36		= 138,
   V3DK_SAVE_VIEW_1	= 139,
   V3DK_SAVE_VIEW_2	= 140,
   V3DK_SAVE_VIEW_3	= 141,
   V3DK_SAVE_VIEW_4	= 142,
   V3DK_SAVE_VIEW_5	= 143,
   V3DK_SAVE_VIEW_6	= 144,
   V3DK_SAVE_VIEW_7	= 145,
   V3DK_SAVE_VIEW_8	= 146,
   V3DK_SAVE_VIEW_9	= 147,
   V3DK_SAVE_VIEW_10= 148,
   V3DK_SAVE_VIEW_11= 149,
   V3DK_SAVE_VIEW_12= 150,
   V3DK_SAVE_VIEW_13= 151,
   V3DK_SAVE_VIEW_14= 152,
   V3DK_SAVE_VIEW_15= 153,
   V3DK_SAVE_VIEW_16= 154,
   V3DK_SAVE_VIEW_17= 155,
   V3DK_SAVE_VIEW_18= 156,
   V3DK_SAVE_VIEW_19= 157,
   V3DK_SAVE_VIEW_20= 158,
   V3DK_SAVE_VIEW_21= 159,
   V3DK_SAVE_VIEW_22= 160,
   V3DK_SAVE_VIEW_23= 161,
   V3DK_SAVE_VIEW_24= 162,
   V3DK_SAVE_VIEW_25= 163,
   V3DK_SAVE_VIEW_26= 164,
   V3DK_SAVE_VIEW_27= 165,
   V3DK_SAVE_VIEW_28= 166,
   V3DK_SAVE_VIEW_29= 167,
   V3DK_SAVE_VIEW_30= 168,
   V3DK_SAVE_VIEW_31= 169,
   V3DK_SAVE_VIEW_32= 170,
   V3DK_SAVE_VIEW_33= 171,
   V3DK_SAVE_VIEW_34= 172,
   V3DK_SAVE_VIEW_35= 173,
   V3DK_SAVE_VIEW_36= 174,
   V3DK_TAB			= 175,
   V3DK_SPACE		= 176,
   V3DK_MENU_1      = 177,
   V3DK_MENU_2      = 178,
   V3DK_MENU_3      = 179,
   V3DK_MENU_4      = 180,
   V3DK_MENU_5      = 181,
   V3DK_MENU_6      = 182,
   V3DK_MENU_7      = 183,
   V3DK_MENU_8      = 184,
   V3DK_MENU_9      = 185,
   V3DK_MENU_10     = 186,
   V3DK_MENU_11     = 187,
   V3DK_MENU_12     = 188,
   V3DK_MENU_13     = 189,
   V3DK_MENU_14     = 190,
   V3DK_MENU_15     = 191,
   V3DK_MENU_16     = 192,
   /* add more here as needed - don't change value of anything that may already be used */
   V3DK_USER        = 0x10000
   };

   static const e3dmouse_virtual_key VirtualKeys[]=
   {
      V3DK_MENU, V3DK_FIT
      , V3DK_TOP, V3DK_LEFT, V3DK_RIGHT, V3DK_FRONT, V3DK_BOTTOM, V3DK_BACK
      , V3DK_ROLL_CW, V3DK_ROLL_CCW
      , V3DK_ISO1, V3DK_ISO2
      , V3DK_1, V3DK_2, V3DK_3, V3DK_4, V3DK_5, V3DK_6, V3DK_7, V3DK_8, V3DK_9, V3DK_10
      , V3DK_ESC, V3DK_ALT, V3DK_SHIFT, V3DK_CTRL
      , V3DK_ROTATE, V3DK_PANZOOM, V3DK_DOMINANT
      , V3DK_PLUS, V3DK_MINUS
      , V3DK_SPIN_CW, V3DK_SPIN_CCW
      , V3DK_TILT_CW, V3DK_TILT_CCW
   };

   static const size_t VirtualKeyCount = numberof(VirtualKeys);
   static const size_t MaxKeyCount = 256; // Arbitary number for sanity checks

   struct tag_VirtualKeyLabel
   {
     e3dmouse_virtual_key vkey;
     const wchar_t* szLabel;
   };

   static const struct tag_VirtualKeyLabel VirtualKeyLabel[] =
   {
     {V3DK_MENU, L"MENU"} , {V3DK_FIT, L"FIT"}
     , {V3DK_TOP, L"T"}, {V3DK_LEFT, L"L"}, {V3DK_RIGHT, L"R"}, {V3DK_FRONT, L"F"}, {V3DK_BOTTOM, L"B"}, {V3DK_BACK, L"BK"}
     , {V3DK_ROLL_CW, L"Roll +"}, {V3DK_ROLL_CCW, L"Roll -"}
     , {V3DK_ISO1, L"ISO1"}, {V3DK_ISO2, L"ISO2"}
     , {V3DK_1, L"1"}, {V3DK_2, L"2"}, {V3DK_3, L"3"}, {V3DK_4, L"4"}, {V3DK_5, L"5"}
     , {V3DK_6, L"6"}, {V3DK_7, L"7"}, {V3DK_8, L"8"}, {V3DK_9, L"9"}, {V3DK_10, L"10"}
     , {V3DK_ESC, L"ESC"}, {V3DK_ALT, L"ALT"}, {V3DK_SHIFT, L"SHIFT"}, {V3DK_CTRL, L"CTRL"}
     , {V3DK_ROTATE, L"Rotate"}, {V3DK_PANZOOM, L"Pan Zoom"}, {V3DK_DOMINANT, L"Dom"}
     , {V3DK_PLUS, L"+"}, {V3DK_MINUS, L"-"}
     , {V3DK_SPIN_CW, L"Spin +"}, {V3DK_SPIN_CCW, L"Spin -"}
     , {V3DK_TILT_CW, L"Tilt +"}, {V3DK_TILT_CCW, L"Tilt -"}
     , {V3DK_ENTER, L"Enter"}, {V3DK_DELETE, L"Delete"}
     , {V3DK_RESERVED0, L"Reserved0"}, {V3DK_RESERVED1, L"Reserved1"}, {V3DK_RESERVED2, L"Reserved2"}
     , {V3DK_F1, L"F1"}, {V3DK_F2, L"F2"}, {V3DK_F3, L"F3"}, {V3DK_F4, L"F4"}, {V3DK_F5, L"F5"}
     , {V3DK_F6, L"F6"}, {V3DK_F7, L"F7"}, {V3DK_F8, L"F8"}, {V3DK_F9, L"F9"}, {V3DK_F10, L"F10"}
     , {V3DK_F11, L"F11"}, {V3DK_F12, L"F12"}, {V3DK_F13, L"F13"}, {V3DK_F14, L"F14"}, {V3DK_F15, L"F15"}
     , {V3DK_F16, L"F16"}, {V3DK_F17, L"F17"}, {V3DK_F18, L"F18"}, {V3DK_F19, L"F19"}, {V3DK_F20, L"F20"}
     , {V3DK_F21, L"F21"}, {V3DK_F22, L"F22"}, {V3DK_F23, L"F23"}, {V3DK_F24, L"F24"}, {V3DK_F25, L"F25"}
     , {V3DK_F26, L"F26"}, {V3DK_F27, L"F27"}, {V3DK_F28, L"F28"}, {V3DK_F29, L"F29"}, {V3DK_F30, L"F30"}
     , {V3DK_F31, L"F31"}, {V3DK_F32, L"F32"}, {V3DK_F33, L"F33"}, {V3DK_F34, L"F34"}, {V3DK_F35, L"F35"}
     , {V3DK_F36, L"F36"}
     , {V3DK_11, L"11"}, {V3DK_12, L"12"}, {V3DK_13, L"13"}, {V3DK_14, L"14"}, {V3DK_15, L"15"}
     , {V3DK_16, L"16"}, {V3DK_17, L"17"}, {V3DK_18, L"18"}, {V3DK_19, L"19"}, {V3DK_20, L"20"}
     , {V3DK_21, L"21"}, {V3DK_22, L"22"}, {V3DK_23, L"23"}, {V3DK_24, L"24"}, {V3DK_25, L"25"}
     , {V3DK_26, L"26"}, {V3DK_27, L"27"}, {V3DK_28, L"28"}, {V3DK_29, L"29"}, {V3DK_30, L"30"}
     , {V3DK_31, L"31"}, {V3DK_32, L"32"}, {V3DK_33, L"33"}, {V3DK_34, L"34"}, {V3DK_35, L"35"}
     , {V3DK_36, L"36"}
     , {V3DK_VIEW_1, L"VIEW 1"}, {V3DK_VIEW_2, L"VIEW 2"}, {V3DK_VIEW_3, L"VIEW 3"}, {V3DK_VIEW_4, L"VIEW 4"}, {V3DK_VIEW_5, L"VIEW 5"}
     , {V3DK_VIEW_6, L"VIEW 6"}, {V3DK_VIEW_7, L"VIEW 7"}, {V3DK_VIEW_8, L"VIEW 8"}, {V3DK_VIEW_9, L"VIEW 9"}, {V3DK_VIEW_10, L"VIEW 10"}
     , {V3DK_VIEW_11, L"VIEW 11"}, {V3DK_VIEW_12, L"VIEW 12"}, {V3DK_VIEW_13, L"VIEW 13"}, {V3DK_VIEW_14, L"VIEW 14"}, {V3DK_VIEW_15, L"VIEW 15"}
     , {V3DK_VIEW_16, L"VIEW 16"}, {V3DK_VIEW_17, L"VIEW 17"}, {V3DK_VIEW_18, L"VIEW 18"}, {V3DK_VIEW_19, L"VIEW 19"}, {V3DK_VIEW_20, L"VIEW 20"}
     , {V3DK_VIEW_21, L"VIEW 21"}, {V3DK_VIEW_22, L"VIEW 22"}, {V3DK_VIEW_23, L"VIEW 23"}, {V3DK_VIEW_24, L"VIEW 24"}, {V3DK_VIEW_25, L"VIEW 25"}
     , {V3DK_VIEW_26, L"VIEW 26"}, {V3DK_VIEW_27, L"VIEW 27"}, {V3DK_VIEW_28, L"VIEW 28"}, {V3DK_VIEW_29, L"VIEW 29"}, {V3DK_VIEW_30, L"VIEW 30"}
     , {V3DK_VIEW_31, L"VIEW 31"}, {V3DK_VIEW_32, L"VIEW 32"}, {V3DK_VIEW_33, L"VIEW 33"}, {V3DK_VIEW_34, L"VIEW 34"}, {V3DK_VIEW_35, L"VIEW 35"}
     , {V3DK_VIEW_36, L"VIEW 36"}
     , {V3DK_SAVE_VIEW_1, L"SAVE VIEW 1"}, {V3DK_SAVE_VIEW_2, L"SAVE VIEW 2"}, {V3DK_SAVE_VIEW_3, L"SAVE VIEW 3"}, {V3DK_SAVE_VIEW_4, L"SAVE VIEW 4"}, {V3DK_SAVE_VIEW_5, L"SAVE VIEW 5"}
     , {V3DK_SAVE_VIEW_6, L"SAVE VIEW 6"}, {V3DK_SAVE_VIEW_7, L"SAVE VIEW 7"}, {V3DK_SAVE_VIEW_8, L"SAVE VIEW 8"}, {V3DK_SAVE_VIEW_9, L"SAVE VIEW 9"}, {V3DK_SAVE_VIEW_10, L"SAVE VIEW 10"}
     , {V3DK_SAVE_VIEW_11, L"SAVE VIEW 11"}, {V3DK_SAVE_VIEW_12, L"SAVE VIEW 12"}, {V3DK_SAVE_VIEW_13, L"SAVE VIEW 13"}, {V3DK_SAVE_VIEW_14, L"SAVE VIEW 14"}, {V3DK_SAVE_VIEW_15, L"SAVE VIEW 15"}
     , {V3DK_SAVE_VIEW_16, L"VIEW 16"}, {V3DK_SAVE_VIEW_17, L"VIEW 17"}, {V3DK_SAVE_VIEW_18, L"VIEW 18"}, {V3DK_SAVE_VIEW_19, L"VIEW 19"}, {V3DK_SAVE_VIEW_20, L"VIEW 20"}
     , {V3DK_SAVE_VIEW_21, L"VIEW 21"}, {V3DK_SAVE_VIEW_22, L"VIEW 22"}, {V3DK_SAVE_VIEW_23, L"VIEW 23"}, {V3DK_SAVE_VIEW_24, L"VIEW 24"}, {V3DK_SAVE_VIEW_25, L"VIEW 25"}
     , {V3DK_SAVE_VIEW_26, L"VIEW 26"}, {V3DK_SAVE_VIEW_27, L"VIEW 27"}, {V3DK_SAVE_VIEW_28, L"VIEW 28"}, {V3DK_SAVE_VIEW_29, L"VIEW 29"}, {V3DK_SAVE_VIEW_30, L"VIEW 30"}
     , {V3DK_SAVE_VIEW_31, L"VIEW 31"}, {V3DK_SAVE_VIEW_32, L"VIEW 32"}, {V3DK_SAVE_VIEW_33, L"VIEW 33"}, {V3DK_SAVE_VIEW_34, L"VIEW 34"}, {V3DK_SAVE_VIEW_35, L"VIEW 35"}
     , {V3DK_SAVE_VIEW_36, L"VIEW 36"}
     , {V3DK_TAB, L"Tab"}, {V3DK_SPACE, L"Space"}
     , {V3DK_MENU_1, L"MENU 1"}, {V3DK_MENU_2, L"MENU 2"}, {V3DK_MENU_3, L"MENU 3"}, {V3DK_MENU_4, L"MENU 4"}
	 , {V3DK_MENU_5, L"MENU 5"}, {V3DK_MENU_6, L"MENU 6"}, {V3DK_MENU_7, L"MENU 7"}, {V3DK_MENU_8, L"MENU 8"}
     , {V3DK_MENU_9, L"MENU 9"}, {V3DK_MENU_10, L"MENU 10"}, {V3DK_MENU_11, L"MENU 11"}, {V3DK_MENU_12, L"MENU 12"}
	 , {V3DK_MENU_13, L"MENU 13"}, {V3DK_MENU_14, L"MENU 14"}, {V3DK_MENU_15, L"MENU 15"}, {V3DK_MENU_16, L"MENU 16"}
     , {V3DK_USER, L"USER"}
   };

   struct tag_VirtualKeyId
   {
     e3dmouse_virtual_key vkey;
     const wchar_t* szId;
   };

   static const struct tag_VirtualKeyId VirtualKeyId[] =
   {
     {V3DK_INVALID, L"V3DK_INVALID"}
     , {V3DK_MENU, L"V3DK_MENU"} , {V3DK_FIT, L"V3DK_FIT"}
     , {V3DK_TOP, L"V3DK_TOP"}, {V3DK_LEFT, L"V3DK_LEFT"}, {V3DK_RIGHT, L"V3DK_RIGHT"}, {V3DK_FRONT, L"V3DK_FRONT"}, {V3DK_BOTTOM, L"V3DK_BOTTOM"}, {V3DK_BACK, L"V3DK_BACK"}
     , {V3DK_ROLL_CW, L"V3DK_ROLL_CW"}, {V3DK_ROLL_CCW, L"V3DK_ROLL_CCW"}
     , {V3DK_ISO1, L"V3DK_ISO1"}, {V3DK_ISO2, L"V3DK_ISO2"}
     , {V3DK_1, L"V3DK_1"}, {V3DK_2, L"V3DK_2"}, {V3DK_3, L"V3DK_3"}, {V3DK_4, L"V3DK_4"}, {V3DK_5, L"V3DK_5"}
     , {V3DK_6, L"V3DK_6"}, {V3DK_7, L"V3DK_7"}, {V3DK_8, L"V3DK_8"}, {V3DK_9, L"V3DK_9"}, {V3DK_10, L"V3DK_10"}
     , {V3DK_ESC, L"V3DK_ESC"}, {V3DK_ALT, L"V3DK_ALT"}, {V3DK_SHIFT, L"V3DK_SHIFT"}, {V3DK_CTRL, L"V3DK_CTRL"}
     , {V3DK_ROTATE, L"V3DK_ROTATE"}, {V3DK_PANZOOM, L"V3DK_PANZOOM"}, {V3DK_DOMINANT, L"V3DK_DOMINANT"}
     , {V3DK_PLUS, L"V3DK_PLUS"}, {V3DK_MINUS, L"V3DK_MINUS"}
     , {V3DK_SPIN_CW, L"V3DK_SPIN_CW"}, {V3DK_SPIN_CCW, L"V3DK_SPIN_CCW"}
     , {V3DK_TILT_CW, L"V3DK_TILT_CW"}, {V3DK_TILT_CCW, L"V3DK_TILT_CCW"}
     , {V3DK_ENTER, L"V3DK_ENTER"}, {V3DK_DELETE, L"V3DK_DELETE"}
     , {V3DK_RESERVED0, L"V3DK_RESERVED0"}, {V3DK_RESERVED1, L"V3DK_RESERVED1"}, {V3DK_RESERVED2, L"V3DK_RESERVED2"}
     , {V3DK_F1, L"V3DK_F1"}, {V3DK_F2, L"V3DK_F2"}, {V3DK_F3, L"V3DK_F3"}, {V3DK_F4, L"V3DK_F4"}, {V3DK_F5, L"V3DK_F5"}
     , {V3DK_F6, L"V3DK_F6"}, {V3DK_F7, L"V3DK_F7"}, {V3DK_F8, L"V3DK_F8"}, {V3DK_F9, L"V3DK_F9"}, {V3DK_F10, L"V3DK_F10"}
     , {V3DK_F11, L"V3DK_F11"}, {V3DK_F12, L"V3DK_F12"}, {V3DK_F13, L"V3DK_F13"}, {V3DK_F14, L"V3DK_F14"}, {V3DK_F15, L"V3DK_F15"}
     , {V3DK_F16, L"V3DK_F16"}, {V3DK_F17, L"V3DK_F17"}, {V3DK_F18, L"V3DK_F18"}, {V3DK_F19, L"V3DK_F19"}, {V3DK_F20, L"V3DK_F20"}
     , {V3DK_F21, L"V3DK_F21"}, {V3DK_F22, L"V3DK_F22"}, {V3DK_F23, L"V3DK_F23"}, {V3DK_F24, L"V3DK_F24"}, {V3DK_F25, L"V3DK_F25"}
     , {V3DK_F26, L"V3DK_F26"}, {V3DK_F27, L"V3DK_F27"}, {V3DK_F28, L"V3DK_F28"}, {V3DK_F29, L"V3DK_F29"}, {V3DK_F30, L"V3DK_F30"}
     , {V3DK_F31, L"V3DK_F31"}, {V3DK_F32, L"V3DK_F32"}, {V3DK_F33, L"V3DK_F33"}, {V3DK_F34, L"V3DK_F34"}, {V3DK_F35, L"V3DK_F35"}
     , {V3DK_F36, L"V3DK_F36"}
     , {V3DK_11, L"V3DK_11"}, {V3DK_12, L"V3DK_12"}, {V3DK_13, L"V3DK_13"}, {V3DK_14, L"V3DK_14"}, {V3DK_15, L"V3DK_15"}
     , {V3DK_16, L"V3DK_16"}, {V3DK_17, L"V3DK_17"}, {V3DK_18, L"V3DK_18"}, {V3DK_19, L"V3DK_19"}, {V3DK_20, L"V3DK_20"}
     , {V3DK_21, L"V3DK_21"}, {V3DK_22, L"V3DK_22"}, {V3DK_23, L"V3DK_23"}, {V3DK_24, L"V3DK_24"}, {V3DK_25, L"V3DK_25"}
     , {V3DK_26, L"V3DK_26"}, {V3DK_27, L"V3DK_27"}, {V3DK_28, L"V3DK_28"}, {V3DK_29, L"V3DK_29"}, {V3DK_30, L"V3DK_30"}
     , {V3DK_31, L"V3DK_31"}, {V3DK_32, L"V3DK_32"}, {V3DK_33, L"V3DK_33"}, {V3DK_34, L"V3DK_34"}, {V3DK_35, L"V3DK_35"}
     , {V3DK_36, L"V3DK_36"}
     , {V3DK_VIEW_1, L"V3DK_VIEW_1"}, {V3DK_VIEW_2, L"V3DK_VIEW_2"}, {V3DK_VIEW_3, L"V3DK_VIEW_3"}, {V3DK_VIEW_4, L"V3DK_VIEW_4"}, {V3DK_VIEW_5, L"V3DK_VIEW_5"}
     , {V3DK_VIEW_6, L"V3DK_VIEW_6"}, {V3DK_VIEW_7, L"V3DK_VIEW_7"}, {V3DK_VIEW_8, L"V3DK_VIEW_8"}, {V3DK_VIEW_9, L"V3DK_VIEW_9"}, {V3DK_VIEW_10, L"V3DK_VIEW_10"}
     , {V3DK_VIEW_11, L"V3DK_VIEW_11"}, {V3DK_VIEW_12, L"V3DK_VIEW_12"}, {V3DK_VIEW_13, L"V3DK_VIEW_13"}, {V3DK_VIEW_14, L"V3DK_VIEW_14"}, {V3DK_VIEW_15, L"V3DK_VIEW_15"}
     , {V3DK_VIEW_16, L"V3DK_VIEW_16"}, {V3DK_VIEW_17, L"V3DK_VIEW_17"}, {V3DK_VIEW_18, L"V3DK_VIEW_18"}, {V3DK_VIEW_19, L"V3DK_VIEW_19"}, {V3DK_VIEW_20, L"V3DK_VIEW_20"}
     , {V3DK_VIEW_21, L"V3DK_VIEW_21"}, {V3DK_VIEW_22, L"V3DK_VIEW_22"}, {V3DK_VIEW_23, L"V3DK_VIEW_23"}, {V3DK_VIEW_24, L"V3DK_VIEW_24"}, {V3DK_VIEW_25, L"V3DK_VIEW_25"}
     , {V3DK_VIEW_26, L"V3DK_VIEW_26"}, {V3DK_VIEW_27, L"V3DK_VIEW_27"}, {V3DK_VIEW_28, L"V3DK_VIEW_28"}, {V3DK_VIEW_29, L"V3DK_VIEW_29"}, {V3DK_VIEW_30, L"V3DK_VIEW_30"}
     , {V3DK_VIEW_31, L"V3DK_VIEW_31"}, {V3DK_VIEW_32, L"V3DK_VIEW_32"}, {V3DK_VIEW_33, L"V3DK_VIEW_33"}, {V3DK_VIEW_34, L"V3DK_VIEW_34"}, {V3DK_VIEW_35, L"V3DK_VIEW_35"}
     , {V3DK_VIEW_36, L"V3DK_VIEW_36"}
     , {V3DK_SAVE_VIEW_1, L"V3DK_SAVE_VIEW_1"}, {V3DK_SAVE_VIEW_2, L"V3DK_SAVE_VIEW_2"}, {V3DK_SAVE_VIEW_3, L"V3DK_SAVE_VIEW_3"}, {V3DK_SAVE_VIEW_4, L"V3DK_SAVE_VIEW_4"}, {V3DK_SAVE_VIEW_5, L"V3DK_SAVE_VIEW_5"}
     , {V3DK_SAVE_VIEW_6, L"V3DK_SAVE_VIEW_6"}, {V3DK_SAVE_VIEW_7, L"V3DK_SAVE_VIEW_7"}, {V3DK_SAVE_VIEW_8, L"V3DK_SAVE_VIEW_8"}, {V3DK_SAVE_VIEW_9, L"V3DK_SAVE_VIEW_9"}, {V3DK_SAVE_VIEW_10, L"V3DK_SAVE_VIEW_10"}
     , {V3DK_SAVE_VIEW_11, L"V3DK_SAVE_VIEW_11"}, {V3DK_SAVE_VIEW_12, L"V3DK_SAVE_VIEW_12"}, {V3DK_SAVE_VIEW_13, L"V3DK_SAVE_VIEW_13"}, {V3DK_SAVE_VIEW_14, L"V3DK_SAVE_VIEW_14"}, {V3DK_SAVE_VIEW_15, L"V3DK_SAVE_VIEW_15"}
     , {V3DK_SAVE_VIEW_16, L"V3DK_SAVE_VIEW_16"}, {V3DK_SAVE_VIEW_17, L"V3DK_SAVE_VIEW_17"}, {V3DK_SAVE_VIEW_18, L"V3DK_SAVE_VIEW_18"}, {V3DK_SAVE_VIEW_19, L"V3DK_SAVE_VIEW_19"}, {V3DK_SAVE_VIEW_20, L"V3DK_SAVE_VIEW_20"}
     , {V3DK_SAVE_VIEW_21, L"V3DK_SAVE_VIEW_21"}, {V3DK_SAVE_VIEW_22, L"V3DK_SAVE_VIEW_22"}, {V3DK_SAVE_VIEW_23, L"V3DK_SAVE_VIEW_23"}, {V3DK_SAVE_VIEW_24, L"V3DK_SAVE_VIEW_24"}, {V3DK_SAVE_VIEW_25, L"V3DK_SAVE_VIEW_25"}
     , {V3DK_SAVE_VIEW_26, L"V3DK_SAVE_VIEW_26"}, {V3DK_SAVE_VIEW_27, L"V3DK_SAVE_VIEW_27"}, {V3DK_SAVE_VIEW_28, L"V3DK_SAVE_VIEW_28"}, {V3DK_SAVE_VIEW_29, L"V3DK_SAVE_VIEW_29"}, {V3DK_SAVE_VIEW_30, L"V3DK_SAVE_VIEW_30"}
     , {V3DK_SAVE_VIEW_31, L"V3DK_SAVE_VIEW_31"}, {V3DK_SAVE_VIEW_32, L"V3DK_SAVE_VIEW_32"}, {V3DK_SAVE_VIEW_33, L"V3DK_SAVE_VIEW_33"}, {V3DK_SAVE_VIEW_34, L"V3DK_SAVE_VIEW_34"}, {V3DK_SAVE_VIEW_35, L"V3DK_SAVE_VIEW_35"}
     , {V3DK_SAVE_VIEW_36, L"V3DK_SAVE_VIEW_36"}
     , {V3DK_TAB, L"V3DK_TAB"}, {V3DK_SPACE, L"V3DK_SPACE"}
     , {V3DK_MENU_1, L"V3DK_MENU_1"}, {V3DK_MENU_2, L"V3DK_MENU_2"}, {V3DK_MENU_3, L"V3DK_MENU_3"}, {V3DK_MENU_4, L"V3DK_MENU_4"}
	 , {V3DK_MENU_5, L"V3DK_MENU_5"}, {V3DK_MENU_6, L"V3DK_MENU_6"}, {V3DK_MENU_7, L"V3DK_MENU_7"}, {V3DK_MENU_8, L"V3DK_MENU_8"}
     , {V3DK_MENU_9, L"V3DK_MENU_9"}, {V3DK_MENU_10, L"V3DK_MENU_10"}, {V3DK_MENU_11, L"V3DK_MENU_11"}, {V3DK_MENU_12, L"V3DK_MENU_12"}
	 , {V3DK_MENU_13, L"V3DK_MENU_13"}, {V3DK_MENU_14, L"V3DK_MENU_14"}, {V3DK_MENU_15, L"V3DK_MENU_15"}, {V3DK_MENU_16, L"V3DK_MENU_16"}
     , {V3DK_USER, L"V3DK_USER"}
   };

   /*-----------------------------------------------------------------------------
   *
   * const  wchar_t* VirtualKeyToId(e3dmouse_virtual_key virtualkey)
   *
   * Args:
   *    virtualkey  the 3dmouse virtual key 
   *
   * Return Value:
   *    Returns a string representation of the standard 3dmouse virtual key, or
   *    an empty string 
   *
   * Description:
   *    Converts a 3dmouse virtual key number to its string identifier
   *
   *---------------------------------------------------------------------------*/
   __inline const wchar_t* VirtualKeyToId(e3dmouse_virtual_key virtualkey)
   {
      if (0 < virtualkey && virtualkey <= numberof(VirtualKeyId) 
        && virtualkey == VirtualKeyId[virtualkey-1].vkey)
        return VirtualKeyId[virtualkey-1].szId;

      for (size_t i=0; i<numberof(VirtualKeyId); ++i)
      {
        if (VirtualKeyId[i].vkey == virtualkey)
          return VirtualKeyId[i].szId;
      }
      return L"";
   }


   /*-----------------------------------------------------------------------------
   *
   * const e3dmouse_virtual_key IdToVirtualKey ( TCHAR *id )
   *
   * Args:
   *    id  - the 3dmouse virtual key ID (a string)
   *
   * Return Value:
   *    The virtual_key number for the id, 
   *    or V3DK_INVALID if it is not a valid tag_VirtualKeyId
   *
   * Description:
   *    Converts a 3dmouse virtual key id (a string) to the V3DK number
   *
   *---------------------------------------------------------------------------*/
   __inline e3dmouse_virtual_key IdToVirtualKey(const wchar_t* id)
   {
      for (size_t i=0; i<sizeof(VirtualKeyId)/sizeof(VirtualKeyId[0]); ++i)
      {
        if (_tcsicmp (VirtualKeyId[i].szId, id) == 0)
          return VirtualKeyId[i].vkey;
      }
      return V3DK_INVALID;
   }


   /*-----------------------------------------------------------------------------
   *
   * const wchar_t* GetKeyLabel(e3dmouse_virtual_key virtualkey)
   *
   * Args:
   *    virtualkey  the 3dmouse virtual key 
   *
   * Return Value:
   *    Returns a string of thye label used on the standard 3dmouse virtual key, or
   *    an empty string 
   *
   * Description:
   *    Converts a 3dmouse virtual key number to its label
   *
   *---------------------------------------------------------------------------*/
   __inline const wchar_t* GetKeyLabel(e3dmouse_virtual_key virtualkey)
   {
      for (size_t i=0; i<numberof(VirtualKeyLabel); ++i)
      {
        if (VirtualKeyLabel[i].vkey == virtualkey)
          return VirtualKeyLabel[i].szLabel;
      }
      return L"";
   }

} // namespace s3dm

namespace tdx {
   enum e3dconnexion_pid {
      eSpacePilot = 0xc625
      , eSpaceNavigator = 0xc626
      , eSpaceExplorer = 0xc627
      , eSpaceNavigatorForNotebooks = 0xc628
      , eSpacePilotPRO = 0xc629
      , eSpaceMousePRO = 0xc62b
      , eSpaceMouseTouch = 0xc62c
	  , eSpaceMouse = 0xc62d
	  , eSpaceMouseWireless = 0xc62e
	  , eSpaceMouseEnterprise = 0xc633
      , eSpaceMouseCompact = 0xc635
   };


   struct tag_VirtualKeys
   {
      e3dconnexion_pid pid;
      size_t nLength;
      s3dm::e3dmouse_virtual_key *vkeys;
      size_t nKeys;
   };

   static const s3dm::e3dmouse_virtual_key SpaceExplorerKeys [] = 
   {
      s3dm::V3DK_INVALID     // there is no button 0
      , s3dm::V3DK_1, s3dm::V3DK_2
      , s3dm::V3DK_TOP, s3dm::V3DK_LEFT, s3dm::V3DK_RIGHT, s3dm::V3DK_FRONT
      , s3dm::V3DK_ESC, s3dm::V3DK_ALT, s3dm::V3DK_SHIFT, s3dm::V3DK_CTRL
      , s3dm::V3DK_FIT, s3dm::V3DK_MENU
      , s3dm::V3DK_PLUS, s3dm::V3DK_MINUS
      , s3dm::V3DK_ROTATE
   };

   static const s3dm::e3dmouse_virtual_key SpacePilotKeys [] = 
   {
      s3dm::V3DK_INVALID 
      , s3dm::V3DK_1, s3dm::V3DK_2, s3dm::V3DK_3, s3dm::V3DK_4, s3dm::V3DK_5, s3dm::V3DK_6
      , s3dm::V3DK_TOP, s3dm::V3DK_LEFT, s3dm::V3DK_RIGHT, s3dm::V3DK_FRONT
      , s3dm::V3DK_ESC, s3dm::V3DK_ALT, s3dm::V3DK_SHIFT, s3dm::V3DK_CTRL
      , s3dm::V3DK_FIT, s3dm::V3DK_MENU
      , s3dm::V3DK_PLUS, s3dm::V3DK_MINUS
      , s3dm::V3DK_DOMINANT, s3dm::V3DK_ROTATE
      , static_cast<s3dm::e3dmouse_virtual_key>(s3dm::V3DK_USER+0x01)
   };

   static const s3dm::e3dmouse_virtual_key SpaceMouseKeys [] = 
   {
      s3dm::V3DK_INVALID 
	  , s3dm::V3DK_MENU, s3dm::V3DK_FIT
   };

   static const s3dm::e3dmouse_virtual_key SpacePilotProKeys [] = 
   {
      s3dm::V3DK_INVALID 
      , s3dm::V3DK_MENU, s3dm::V3DK_FIT
      , s3dm::V3DK_TOP, s3dm::V3DK_LEFT, s3dm::V3DK_RIGHT, s3dm::V3DK_FRONT, s3dm::V3DK_BOTTOM, s3dm::V3DK_BACK
      , s3dm::V3DK_ROLL_CW, s3dm::V3DK_ROLL_CCW
      , s3dm::V3DK_ISO1, s3dm::V3DK_ISO2
      , s3dm::V3DK_1, s3dm::V3DK_2, s3dm::V3DK_3, s3dm::V3DK_4, s3dm::V3DK_5
      , s3dm::V3DK_6, s3dm::V3DK_7, s3dm::V3DK_8, s3dm::V3DK_9, s3dm::V3DK_10
      , s3dm::V3DK_ESC, s3dm::V3DK_ALT, s3dm::V3DK_SHIFT, s3dm::V3DK_CTRL
      , s3dm::V3DK_ROTATE, s3dm::V3DK_PANZOOM, s3dm::V3DK_DOMINANT
      , s3dm::V3DK_PLUS, s3dm::V3DK_MINUS
   };

   static const s3dm::e3dmouse_virtual_key SpaceMouseProKeys [] = 
   {
      s3dm::V3DK_INVALID 
      , s3dm::V3DK_MENU, s3dm::V3DK_FIT
      , s3dm::V3DK_TOP, s3dm::V3DK_INVALID, s3dm::V3DK_RIGHT, s3dm::V3DK_FRONT, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID
      , s3dm::V3DK_ROLL_CW, s3dm::V3DK_INVALID
      , s3dm::V3DK_INVALID, s3dm::V3DK_INVALID
      , s3dm::V3DK_1, s3dm::V3DK_2, s3dm::V3DK_3, s3dm::V3DK_4, s3dm::V3DK_INVALID
      , s3dm::V3DK_INVALID, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID
      , s3dm::V3DK_ESC, s3dm::V3DK_ALT, s3dm::V3DK_SHIFT, s3dm::V3DK_CTRL
      , s3dm::V3DK_ROTATE
   };

   static const s3dm::e3dmouse_virtual_key SpaceMouseTouchKeys [] = 
   {
      s3dm::V3DK_INVALID 
      , s3dm::V3DK_MENU, s3dm::V3DK_FIT
      , s3dm::V3DK_TOP, s3dm::V3DK_LEFT, s3dm::V3DK_RIGHT, s3dm::V3DK_FRONT, s3dm::V3DK_BOTTOM, s3dm::V3DK_BACK
      , s3dm::V3DK_ROLL_CW, s3dm::V3DK_ROLL_CCW
      , s3dm::V3DK_ISO1, s3dm::V3DK_ISO2
      , s3dm::V3DK_1, s3dm::V3DK_2, s3dm::V3DK_3, s3dm::V3DK_4, s3dm::V3DK_5
      , s3dm::V3DK_6, s3dm::V3DK_7, s3dm::V3DK_8, s3dm::V3DK_9, s3dm::V3DK_10
   };

   static const s3dm::e3dmouse_virtual_key SpaceMouseEnterpriseKeys [] = 
   {
      s3dm::V3DK_INVALID 
      , s3dm::V3DK_MENU, s3dm::V3DK_FIT
      , s3dm::V3DK_TOP, s3dm::V3DK_LEFT, s3dm::V3DK_RIGHT, s3dm::V3DK_FRONT, s3dm::V3DK_BOTTOM, s3dm::V3DK_BACK
      , s3dm::V3DK_ROLL_CW, s3dm::V3DK_ROLL_CCW
      , s3dm::V3DK_ISO1, s3dm::V3DK_ISO2
      , s3dm::V3DK_1, s3dm::V3DK_2, s3dm::V3DK_3, s3dm::V3DK_4, s3dm::V3DK_5
      , s3dm::V3DK_6, s3dm::V3DK_7, s3dm::V3DK_8, s3dm::V3DK_9, s3dm::V3DK_10
      , s3dm::V3DK_ESC, s3dm::V3DK_ALT, s3dm::V3DK_SHIFT, s3dm::V3DK_CTRL
      , s3dm::V3DK_ROTATE
      , /* 28 */ s3dm::V3DK_INVALID, s3dm::V3DK_INVALID
      , /* 30 */ s3dm::V3DK_INVALID, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID, s3dm::V3DK_INVALID
      , /* 35 */ s3dm::V3DK_INVALID
      , s3dm::V3DK_ENTER
      , s3dm::V3DK_DELETE
      , s3dm::V3DK_INVALID
      , s3dm::V3DK_INVALID
      , /* 40 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 45 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 50 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 55 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 60 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 65 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 70 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 75 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , s3dm::V3DK_11, s3dm::V3DK_12
      , s3dm::V3DK_INVALID
      , /* 80 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 85 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 90 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 95 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 100 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID, s3dm::V3DK_INVALID
      , s3dm::V3DK_VIEW_1,s3dm::V3DK_VIEW_2,s3dm::V3DK_VIEW_3
      , /* 106 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 110 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 115 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 120 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 125 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 130 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 135 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , s3dm::V3DK_SAVE_VIEW_1,s3dm::V3DK_SAVE_VIEW_2,s3dm::V3DK_SAVE_VIEW_3
      , /* 142 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 145 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 150 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 155 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 160 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 165 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , /* 170 */ s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID,s3dm::V3DK_INVALID
      , s3dm::V3DK_TAB,s3dm::V3DK_SPACE
   };

   static const struct tag_VirtualKeys _3dmouseHID2VirtualKeys[]= 
   {
      eSpacePilot
      , numberof(SpacePilotKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpacePilotKeys)
      , numberof(SpacePilotKeys)-1

      , eSpaceExplorer
      , numberof(SpaceExplorerKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceExplorerKeys)
      , numberof(SpaceExplorerKeys)-1

      , eSpaceNavigator
      , numberof(SpaceMouseKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseKeys)
      , numberof(SpaceMouseKeys)-1
      
	   , eSpaceNavigatorForNotebooks
	   , numberof(SpaceMouseKeys)
	   , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseKeys)
	   , numberof(SpaceMouseKeys) - 1

	   , eSpaceMouseWireless
	   , numberof(SpaceMouseKeys)
	   , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseKeys)
	   , numberof(SpaceMouseKeys) - 1

	   , eSpaceMouseCompact
	   , numberof(SpaceMouseKeys)
	   , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseKeys)
	   , numberof(SpaceMouseKeys) - 1

	   , eSpacePilotPRO
      , numberof(SpacePilotProKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpacePilotProKeys)
      , numberof(SpacePilotProKeys)-1

      , eSpaceMousePRO
      , numberof(SpaceMouseProKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseProKeys)
      , 15  // HACK: ? why is this hard-coded to less than numberof(SpaceMouseProKeys)?

      , eSpaceMouse
      , numberof(SpaceMouseKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseKeys)
      , numberof(SpaceMouseKeys)-1

      , eSpaceMouseTouch
      , numberof(SpaceMouseTouchKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseTouchKeys)
      , numberof(SpaceMouseTouchKeys)-1

      , eSpaceMouseEnterprise
      , numberof(SpaceMouseEnterpriseKeys)
      , const_cast<s3dm::e3dmouse_virtual_key *>(SpaceMouseEnterpriseKeys)
      , numberof(SpaceMouseEnterpriseKeys)-1
   };



   /*-----------------------------------------------------------------------------
   *
   * unsigned short HidToVirtualKey(unsigned short pid, unsigned short hidKeyCode)
   *
   * Args:
   *    pid - USB Product ID (PID) of 3D mouse device 
   *    hidKeyCode - Hid keycode as retrieved from a Raw Input packet
   *
   * Return Value:
   *    Returns the standard 3d mouse virtual key (button identifier) or zero if an error occurs.
   *
   * Description:
   *    Converts a hid device keycode (button identifier) of a pre-2009 3Dconnexion USB device
   *    to the standard 3d mouse virtual key definition.
   *
   *---------------------------------------------------------------------------*/
   __inline unsigned short HidToVirtualKey(unsigned long pid, unsigned short hidKeyCode)
   {
      unsigned short virtualkey=hidKeyCode;
      for (size_t i=0; i<numberof(_3dmouseHID2VirtualKeys); ++i)
      {
         if (pid == _3dmouseHID2VirtualKeys[i].pid)
         {
            if (hidKeyCode < _3dmouseHID2VirtualKeys[i].nLength)
               virtualkey = _3dmouseHID2VirtualKeys[i].vkeys[hidKeyCode];

//          Change 10/24/2012: if the key doesn't need translating then pass it through
//            else
//              virtualkey = s3dm::V3DK_INVALID;
            break;
         }
      }
      // Remaining devices are unchanged
#if _TRACE_VIRTUAL_KEYS
     TRACE(L"Converted %d to %s(=%d) for pid 0x%x\n", hidKeyCode, VirtualKeyToId(virtualkey), virtualkey, pid);
#endif
      return virtualkey;
   }

   /*-----------------------------------------------------------------------------
   *
   * unsigned short VirtualKeyToHid(unsigned short pid, unsigned short virtualkey)
   *
   * Args:
   *    pid - USB Product ID (PID) of 3D mouse device 
   *    virtualkey - standard 3d mouse virtual key
   *
   * Return Value:
   *    Returns the Hid keycode as retrieved from a Raw Input packet
   *
   * Description:
   *    Converts a standard 3d mouse virtual key definition 
   *    to the hid device keycode (button identifier).
   *
   *---------------------------------------------------------------------------*/
   __inline unsigned short VirtualKeyToHid(unsigned long pid, unsigned short virtualkey)
   {
      unsigned short hidKeyCode = virtualkey;
      for (size_t i=0; i<numberof(_3dmouseHID2VirtualKeys); ++i)
      {
         if (pid == _3dmouseHID2VirtualKeys[i].pid)
         {
            for (unsigned short hidCode=0; hidCode<_3dmouseHID2VirtualKeys[i].nLength; ++hidCode)
            {
              if (virtualkey==_3dmouseHID2VirtualKeys[i].vkeys[hidCode])
                return hidCode;
            }
//          Change 10/24/2012: if the key doesn't need translating then pass it through
            return hidKeyCode;
         }
      }
      // Remaining devices are unchanged
#if _TRACE_VIRTUAL_KEYS
     TRACE(L"Converted %d to %s(=%d) for pid 0x%x\n", virtualkey, VirtualKeyToId(virtualkey), hidKeyCode, pid);
#endif
      return hidKeyCode;
   }

   /*-----------------------------------------------------------------------------
   *
   * unsigned int NumberOfButtons(unsigned short pid)
   *
   * Args:
   *    pid - USB Product ID (PID) of 3D mouse device 
   *
   * Return Value:
   *    Returns the number of buttons of the device.
   *
   * Description:
   *   Returns the number of buttons of the device.
   *
   *---------------------------------------------------------------------------*/
   __inline size_t NumberOfButtons(unsigned long pid)
   {
      for (size_t i=0; i<numberof(_3dmouseHID2VirtualKeys); ++i)
      {
         if (pid == _3dmouseHID2VirtualKeys[i].pid)
            return _3dmouseHID2VirtualKeys[i].nKeys;
      }
      return 0;
   }

   /*-----------------------------------------------------------------------------
   *
   * int HidToIndex(unsigned short pid, unsigned short hidKeyCode)
   *
   * Args:
   *    pid - USB Product ID (PID) of 3D mouse device 
   *    hidKeyCode - Hid keycode as retrieved from a Raw Input packet
   *
   * Return Value:
   *    Returns the index of the hid button or -1 if an error occurs.
   *
   * Description:
   *    Converts a hid device keycode (button identifier) to a zero based 
   *    sequential index.
   *
   *---------------------------------------------------------------------------*/
   __inline int HidToIndex(unsigned long pid, unsigned short hidKeyCode)
   {
      for (size_t i=0; i<numberof(_3dmouseHID2VirtualKeys); ++i)
      {
         if (pid == _3dmouseHID2VirtualKeys[i].pid)
         {
            int index=-1;
            if (hidKeyCode < _3dmouseHID2VirtualKeys[i].nLength)
            {
              unsigned short virtualkey = _3dmouseHID2VirtualKeys[i].vkeys[hidKeyCode];
              if (virtualkey != s3dm::V3DK_INVALID)
              {
                for (int key=1; key<=hidKeyCode; ++key)
                {
                  if (_3dmouseHID2VirtualKeys[i].vkeys[key] != s3dm::V3DK_INVALID)
                    ++index;
                }
              }
            }
            return index;
         }
      }
      return hidKeyCode-1;
   }

   /*-----------------------------------------------------------------------------
   *
   * unsigned short IndexToHid(unsigned short pid, int index)
   *
   * Args:
   *    pid - USB Product ID (PID) of 3D mouse device 
   *    index - index of button
   *
   * Return Value:
   *    Returns the Hid keycode of the nth button or 0 if an error occurs.
   *
   * Description:
   *    Returns the hid device keycode of the nth button
   *
   *---------------------------------------------------------------------------*/
   __inline unsigned short IndexToHid(unsigned long pid, int index)
   {
      if (index < 0)
        return 0;
      for (size_t i=0; i<numberof(_3dmouseHID2VirtualKeys); ++i)
      {
         if (pid == _3dmouseHID2VirtualKeys[i].pid)
         {
            if (index < static_cast<int>(_3dmouseHID2VirtualKeys[i].nLength))
            {
              for (size_t key=1; key<_3dmouseHID2VirtualKeys[i].nLength; ++key)
              {
                if (_3dmouseHID2VirtualKeys[i].vkeys[key] != s3dm::V3DK_INVALID)
                {
                  --index;
                  if (index == -1)
                    return static_cast<unsigned short>(key);
                }
              }
            }
            return 0;
         }
      }
      return static_cast<unsigned short>(index+1);
   }

}; //namespace tdx
#endif // virtualkeys_HPP_INCLUDED_
