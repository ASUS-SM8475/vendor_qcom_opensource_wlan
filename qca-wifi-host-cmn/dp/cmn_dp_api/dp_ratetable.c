/*
 * Copyright (c) 2016-2019, 2021 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "dp_ratetable.h"
#include "qdf_module.h"
#include "cdp_txrx_mon_struct.h"

enum {
	MODE_11A        = 0,   /* 11a Mode */
	MODE_11G        = 1,   /* 11b/g Mode */
	MODE_11B        = 2,   /* 11b Mode */
	MODE_11GONLY    = 3,   /* 11g only Mode */
	MODE_11NA_HT20   = 4,  /* 11a HT20 mode */
	MODE_11NG_HT20   = 5,  /* 11g HT20 mode */
	MODE_11NA_HT40   = 6,  /* 11a HT40 mode */
	MODE_11NG_HT40   = 7,  /* 11g HT40 mode */
	MODE_11AC_VHT20 = 8,
	MODE_11AC_VHT40 = 9,
	MODE_11AC_VHT80 = 10,
	MODE_11AC_VHT20_2G = 11,
	MODE_11AC_VHT40_2G = 12,
	MODE_11AC_VHT80_2G = 13,
	MODE_11AC_VHT80_80 = 14,
	MODE_11AC_VHT160   = 15,
	MODE_11AX_HE20 = 16,
	MODE_11AX_HE40 = 17,
	MODE_11AX_HE80 = 18,
	MODE_11AX_HE80_80 = 19,
	MODE_11AX_HE160 = 20,
	MODE_11AX_HE20_2G = 21,
	MODE_11AX_HE40_2G = 22,
	MODE_11AX_HE80_2G = 23,
	/* MODE_UNKNOWN should not be used within the host / target interface.
	 * Thus, it is permissible for ODE_UNKNOWN to be conditionally-defined,
	 * taking different values when compiling for different targets.
	 */
	MODE_UNKNOWN,
	MODE_UNKNOWN_NO_160MHZ_SUPPORT = 14, /* not needed? */
	MODE_UNKNOWN_160MHZ_SUPPORT = MODE_UNKNOWN, /* not needed? */
} DP_PHY_MODE;

/* The following would span more than one octet
 * when 160MHz BW defined for VHT
 * Also it's important to maintain the ordering of this enum
 * else it would break other rate adapation functions
 */

enum DP_CMN_RATECODE_PREAM_TYPE {
	DP_CMN_RATECODE_PREAM_OFDM,
	DP_CMN_RATECODE_PREAM_CCK,
	DP_CMN_RATECODE_PREAM_HT,
	DP_CMN_RATECODE_PREAM_VHT,
	DP_CMN_RATECODE_PREAM_HE,
	DP_CMN_RATECODE_PREAM_COUNT,
};

/*
 * @validmodemask : bit mask where 1 indicates the rate is valid for that mode
 * @DP_CMN_MODULATION : modulation CCK/OFDM/MCS
 * @propmask : bit mask of rate property. NSS/STBC/TXBF/LDPC
 * @ratekbps : Rate in Kbits per second
 * @ratebpdsgi : Rate in kbits per second if HT SGI is enabled
 * @ratekbpsdgi : Rate in kbits per second if 1.6us GI is enabled
 * @ratekbpsqgi : Rate in kbits per second if 3.2us GI is enabled
 * @ratekbpsdcm : Rate in kbits per second if DCM is applied
 * @userratekabps : User rate in KBits per second
 * @dot11rate : Value that goes into supported rates info element of MLME
 * @ratecode : rate that goes into hw descriptors
 */
const struct DP_CMN_RATE_TABLE {
	struct {
		uint32_t validmodemask;
		enum DP_CMN_MODULATION_TYPE phy;
		uint32_t ratekbps;
		uint32_t ratekbpssgi;
		uint32_t ratekbpsdgi;
		uint32_t ratekbpsqgi;
		uint32_t ratekbpsdcm;
		uint32_t userratekbps;
		uint16_t ratecode;
	} info[DP_RATE_TABLE_SIZE];
} DP_CMN_RATE_TABLE;

/*Use the highest bit to indicate the invalid bcc rates accorss
 *different PHYMODE
 */
#define INVALID_BCC_RATE BIT(MODE_UNKNOWN)

#define CCK_MODE_VALID_MASK ((1 << MODE_11G) | (1 << MODE_11B) | \
		(1 << MODE_11NG_HT20) | (1 << MODE_11NG_HT40) | \
		(1 << MODE_11AC_VHT40_2G) | (1 << MODE_11AC_VHT20_2G) |\
		(1 << MODE_11AC_VHT80_2G))

#define OFDM_MODE_VALID_MASK ((1 << MODE_11A) | (1 << MODE_11G) | \
		(1 << MODE_11GONLY) | (1 << MODE_11NA_HT20) | \
		(1 << MODE_11NG_HT20) \
		| (1 << MODE_11NA_HT40) | (1 << MODE_11NG_HT40) \
		| (1 << MODE_11AC_VHT40) | (1 << MODE_11AC_VHT20) | \
		(1 << MODE_11AC_VHT80) \
		| (1 << MODE_11AC_VHT40_2G) | (1 << MODE_11AC_VHT20_2G) | \
		(1 << MODE_11AC_VHT80_2G) \
		| (1 << MODE_11AC_VHT160) | (1 << MODE_11AC_VHT80_80))

#define HT20_MODE_VALID_MASK ((1 << MODE_11NA_HT20) | \
		(1 << MODE_11NG_HT20) \
		| (1 << MODE_11NA_HT40) | (1 << MODE_11NG_HT40) \
		| (1 << MODE_11AC_VHT40) | (1 << MODE_11AC_VHT20) | \
		(1 << MODE_11AC_VHT80) \
		| (1 << MODE_11AC_VHT40_2G) | (1 << MODE_11AC_VHT20_2G) | \
		(1 << MODE_11AC_VHT80_2G) \
		| (1 << MODE_11AC_VHT160) | (1 << MODE_11AC_VHT80_80))

#define HT40_MODE_VALID_MASK ((1 << MODE_11NA_HT40) | \
		(1 << MODE_11NG_HT40) \
		| (1 << MODE_11AC_VHT40) | (1 << MODE_11AC_VHT80) \
		| (1 << MODE_11AC_VHT40_2G) | (1 << MODE_11AC_VHT80_2G) \
		| (1 << MODE_11AC_VHT160) | (1 << MODE_11AC_VHT80_80))

#define VHT20_MODE_VALID_MASK ((1 << MODE_11AC_VHT20) | \
		(1 << MODE_11AC_VHT40) | (1 << MODE_11AC_VHT80) | \
		(1 << MODE_11AC_VHT40_2G) | (1 << MODE_11AC_VHT20_2G) | \
		(1 << MODE_11AC_VHT80_2G) | \
		(1 << MODE_11AC_VHT160) | (1 << MODE_11AC_VHT80_80))

#define VHT40_MODE_VALID_MASK ((1 << MODE_11AC_VHT40) | \
		(1 << MODE_11AC_VHT80) | \
		(1 << MODE_11AC_VHT40_2G) | (1 << MODE_11AC_VHT80_2G) | \
		(1 << MODE_11AC_VHT160) | (1 << MODE_11AC_VHT80_80))

#define VHT80_MODE_VALID_MASK ((1 << MODE_11AC_VHT80) | \
		(1 << MODE_11AC_VHT80_2G) | \
		(1 << MODE_11AC_VHT160) | (1 << MODE_11AC_VHT80_80))

#define VHT160_MODE_VALID_MASK ((1 << MODE_11AC_VHT160) | \
		(1 << MODE_11AC_VHT80_80))

#define VHT20_LDPC_ONLY_MASKS (VHT20_MODE_VALID_MASK | INVALID_BCC_RATE)
#define VHT40_LDPC_ONLY_MASKS (VHT40_MODE_VALID_MASK | INVALID_BCC_RATE)
#define VHT80_LDPC_ONLY_MASKS (VHT80_MODE_VALID_MASK | INVALID_BCC_RATE)
#define VHT160_LDPC_ONLY_MASKS (VHT160_MODE_VALID_MASK | INVALID_BCC_RATE)

#define VHT_INVALID_MCS (0xFF)
#define VHT_INVALID_RATES_MASK 0

#define HE20_MODE_VALID_MASK ((1 << MODE_11AX_HE20) |\
		(1 << MODE_11AX_HE40) | \
		(1 << MODE_11AX_HE80) | (1 << MODE_11AX_HE20_2G) | \
		(1 << MODE_11AX_HE40_2G) | \
		(1 << MODE_11AX_HE80_2G) | (1 << MODE_11AX_HE80_80) | \
		(1 << MODE_11AX_HE160))

#define HE40_MODE_VALID_MASK ((1 << MODE_11AX_HE40) | \
		(1 << MODE_11AX_HE80) | (1 << MODE_11AX_HE40_2G) | \
		(1 << MODE_11AX_HE80_2G) | (1 << MODE_11AX_HE80_80) | \
		(1 << MODE_11AX_HE160))

#define HE80_MODE_VALID_MASK ((1 << MODE_11AX_HE80) | \
		(1 << MODE_11AX_HE80_2G) | \
		(1 << MODE_11AX_HE80_80) | (1 << MODE_11AX_HE160))

#define HE160_MODE_VALID_MASK ((1 << MODE_11AX_HE80_80) | \
		(1 << MODE_11AX_HE160))

#define HE20_LDPC_ONLY_MASKS (HE20_MODE_VALID_MASK | INVALID_BCC_RATE)
#define HE40_LDPC_ONLY_MASKS (HE40_MODE_VALID_MASK | INVALID_BCC_RATE)
#define HE80_LDPC_ONLY_MASKS (HE80_MODE_VALID_MASK | INVALID_BCC_RATE)
#define HE160_LDPC_ONLY_MASKS (HE160_MODE_VALID_MASK | INVALID_BCC_RATE)

#define HE_INVALID_RATES_MASK 0

static const struct DP_CMN_RATE_TABLE dp_11abgnratetable = {
	{
	  /* When number of spatial strams > 4 or 11AX support is enabled */

	  /*     0  11 Mb  */ { CCK_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_CCK,
		  11000,    11000,        0,        0,        0,    11000,
		  0x100  },
	  /*     1 5.5 Mb  */ { CCK_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_CCK,
		  5500,     5500,        0,        0,        0,     5500,  0x101
	  },
	  /*     2   2 Mb  */ { CCK_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_CCK,
		  2000,     2000,        0,        0,        0,     2000,  0x102
	  },
	  /*     3   1 Mb  */ { CCK_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_CCK,
		  1000,     1000,        0,        0,        0,     1000,  0x103
	  },
	  /*   4  48 Mb  */ { OFDM_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_OFDM,
		  48000,     48000,         0,         0,         0,     48000,
		  0x000  },
	  /*   5  24 Mb  */ { OFDM_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_OFDM,
		  24000,     24000,         0,         0,         0,     24000,
		  0x001  },
	  /*   6  12 Mb  */ { OFDM_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_OFDM,
		  12000,     12000,         0,         0,         0,     12000,
		  0x002  },
	  /*   7   6 Mb  */ { OFDM_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_OFDM,
		  6000,      6000,         0,         0,         0,      6000,
		  0x003 },
	  /*   8  54 Mb  */ { OFDM_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_OFDM,
		  54000,     54000,         0,         0,         0,     54000,
		  0x004  },
	  /*   9  36 Mb  */ { OFDM_MODE_VALID_MASK, DP_CMN_MOD_IEEE80211_T_OFDM,
		  36000,     36000,         0,         0,         0,     36000,
		  0x005  },
	  /*     10  18 Mb  */ { OFDM_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_OFDM,
		  18000,     18000,         0,         0,         0,     18000,
		  0x006  },
	  /*     11   9 Mb  */ { OFDM_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_OFDM,
		  9000,      9000,         0,         0,         0,      9000,
		  0x007},

	  /*      12 MCS-00 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                            6500,
		  7200,        0,        0,        0,     6500,  0x200   },
	  /*      13 MCS-01 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           13000,
		  14400,        0,        0,        0,    13000,  0x201   },
	  /*      14 MCS-02 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           19500,
		  21700,        0,        0,        0,    19500,  0x202   },
	  /*      15 MCS-03 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           26000,
		  28900,        0,        0,        0,    26000,  0x203   },
	  /*      16 MCS-04 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           39000,
		  43300,        0,        0,        0,    39000,  0x204   },
	  /*      17 MCS-05 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           52000,
		  57800,        0,        0,        0,    52000,  0x205   },
	  /*      18 MCS-06 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           58500,
		  65000,        0,        0,        0,    58500,  0x206   },
	  /*      19 MCS-07 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           65000,
		  72200,        0,        0,        0,    65000,  0x207   },
	  /* When number of spatial streams > 1 */
	  /*      20 MCS-00 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           13000,
		  14400,        0,        0,        0,    13000,  0x220   },
	  /*      21 MCS-01 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           26000,
		  28900,        0,        0,        0,    26000,  0x221   },
	  /*      22 MCS-02 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           39000,
		  43300,        0,        0,        0,    39000,  0x222   },
	  /*      23 MCS-03 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           52000,
		  57800,        0,        0,        0,    52000,  0x223   },
	  /*      24 MCS-04 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           78000,
		  86700,        0,        0,        0,    78000,  0x224   },
	  /*      25 MCS-05 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          104000,
		  115600,        0,        0,        0,   104000,  0x225   },
	  /*      26 MCS-06 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          117000,
		  130000,        0,        0,        0,   117000,  0x226   },
	  /*      27 MCS-07 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          130000,
		  144000,        0,        0,        0,   130000,  0x227   },
	  /* When number of spatial streams > 2 */
	  /*      28 MCS-00 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           19500,
		  21700,        0,        0,        0,    19500,  0x240   },
	  /*      29 MCS-01 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           39000,
		  43300,        0,        0,        0,    39000,  0x241   },
	  /*      30 MCS-02 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           58500,
		  65000,        0,        0,        0,    58500,  0x242   },
	  /*      31 MCS-03 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           78000,
		  86700,        0,        0,        0,    78000,  0x243   },
	  /*      32 MCS-04 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          117000,
		  130000,        0,        0,        0,   117000,  0x244   },
	  /*      33 MCS-05 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          156000,
		  173300,        0,        0,        0,   156000,  0x245   },
	  /*      34 MCS-06 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          175500,
		  195000,        0,        0,        0,   175500,  0x246   },
	  /*      35 MCS-07 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          195000,
		  216700,        0,        0,        0,   195000,  0x247   },
	  /* When number of spatial streams > 3 */
	  /*      36 MCS-00 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           26000,
		  28900,        0,        0,        0,    26000,  0x260   },
	  /*      37 MCS-01 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           52000,
		  57800,        0,        0,        0,    52000,  0x261   },
	  /*      38 MCS-02 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                           78000,
		  86700,        0,        0,        0,    78000,  0x262   },
	  /*      39 MCS-03 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          104000,
		  115600,        0,        0,        0,   104000,  0x263   },
	  /*      40 MCS-04 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          156000,
		  173300,        0,        0,        0,   156000,  0x264   },
	  /*      41 MCS-05 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          208000,
		  231100,        0,        0,        0,   208000,  0x265   },
	  /*      42 MCS-06 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          234000,
		  260000,        0,        0,        0,   234000,  0x266   },
	  /*      43 MCS-07 */ { HT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_20,                          260000,
		  288900,        0,        0,        0,   260000,  0x267   },

	  /* 11n HT40 rates                                                   */
	  /*      44 MCS-00 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           13500,
		  15000,        0,        0,        0,    13500,  0x200  },
	  /*      45 MCS-01 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           27000,
		  30000,        0,        0,        0,    27000,  0x201  },
	  /*      46 MCS-02 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           40500,
		  45000,        0,        0,        0,    40500,  0x202  },
	  /*      47 MCS-03 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           54000,
		  60000,        0,        0,        0,    54000,  0x203  },
	  /*      48 MCS-04 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           81500,
		  90000,        0,        0,        0,    81500,  0x204  },
	  /*      49 MCS-05 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          108000,
		  120000,        0,        0,        0,   108000,  0x205  },
	  /*      50 MCS-06 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          121500,
		  135000,        0,        0,        0,   121500,  0x206  },
	  /*      51 MCS-07 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          135000,
		  150000,        0,        0,        0,   135000,  0x207  },
	  /* When number of spatial streams > 1 */
	  /*      52 MCS-00 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           27000,
		  30000,        0,        0,        0,    27000,  0x220  },
	  /*      53 MCS-01 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           54000,
		  60000,        0,        0,        0,    54000,  0x221  },
	  /*      54 MCS-02 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           81000,
		  90000,        0,        0,        0,    81000,  0x222  },
	  /*      55 MCS-03 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          108000,
		  120000,        0,        0,        0,   108000,  0x223  },
	  /*      56 MCS-04 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          162000,
		  180000,        0,        0,        0,   162000,  0x224  },
	  /*      57 MCS-05 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          216000,
		  240000,        0,        0,        0,   216000,  0x225  },
	  /*      58 MCS-06 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          243000,
		  270000,        0,        0,        0,   243000,  0x226  },
	  /*      59 MCS-07 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          270000,
		  300000,        0,        0,        0,   270000,  0x227  },
	  /* When number of spatial streams > 2 */
	  /*      60 MCS-00 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           40500,
		  45000,        0,        0,        0,    40500,  0x240  },
	  /*      61 MCS-01 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           81000,
		  90000,        0,        0,        0,    81000,  0x241  },
	  /*      62 MCS-02 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          121500,
		  135000,        0,        0,        0,   121500,  0x242  },
	  /*      63 MCS-03 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          162000,
		  180000,        0,        0,        0,   162000,  0x243  },
	  /*      64 MCS-04 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          243000,
		  270000,        0,        0,        0,   243000,  0x244  },
	  /*      65 MCS-05 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          324000,
		  360000,        0,        0,        0,   324000,  0x245  },
	  /*      66 MCS-06 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          364500,
		  405000,        0,        0,        0,   364500,  0x246  },
	  /*      67 MCS-07 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          405000,
		  450000,        0,        0,        0,   405000,  0x247  },
	  /* When number of spatial streams > 3 */
	  /*      68 MCS-00 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                           54000,
		  60000,        0,        0,        0,    54000,  0x260  },
	  /*      69 MCS-01 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          108000,
		  120000,        0,        0,        0,   108000,  0x261  },
	  /*      70 MCS-02 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          162000,
		  180000,        0,        0,        0,   162000,  0x262  },
	  /*      71 MCS-03 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          216000,
		  240000,        0,        0,        0,   216000,  0x263  },
	  /*      72 MCS-04 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          324000,
		  360000,        0,        0,        0,   324000,  0x264  },
	  /*      73 MCS-05 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          432000,
		  480000,        0,        0,        0,   432000,  0x265  },
	  /*      74 MCS-06 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          486000,
		  540000,        0,        0,        0,   486000,  0x266  },
	  /*      75 MCS-07 */ { HT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HT_40,                          540000,
		  600000,        0,        0,        0,   540000,  0x267  },

	  /* 11ac VHT20 rates                                                 */
	  /*      76 MCS-00 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                           6500,
		  7200,        0,        0,        0,     6500,  0x300  },
	  /*      77 MCS-01 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          13000,
		  14400,        0,        0,        0,    13000,  0x301  },
	  /*      78 MCS-02 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          19500,
		  21700,        0,        0,        0,    19500,  0x302  },
	  /*      79 MCS-03 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          26000,
		  28900,        0,        0,        0,    26000,  0x303  },
	  /*      80 MCS-04 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          39000,
		  43300,        0,        0,        0,    39000,  0x304  },
	  /*      81 MCS-05 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          52000,
		  57800,        0,        0,        0,    52000,  0x305  },
	  /*      82 MCS-06 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          58500,
		  65000,        0,        0,        0,    58500,  0x306  },
	  /*      83 MCS-07 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          65000,
		  72200,        0,        0,        0,    65000,  0x307  },
	  /*      84 MCS-08 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          78000,
		  86700,        0,        0,        0,    78000,  0x308  },
	  /*      85 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          86500,
		  96000,        0,        0,        0,    86500,  0x309 },
	  /* When we support very hight throughput MCS */
	  /* 86 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  97500,   108300,	    0,        0,	 0,
		  97500,  0x30a},
	  /*	     87 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  108300,  120400,	    0,        0,	 0,
		  108300,  0x30b},

	  /* When number of spatial streams > 1 */
	  /*      88 MCS-00 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          13000,
		  14400,        0,        0,        0,    13000,  0x320 },
	  /*      89 MCS-01 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          26000,
		  28900,        0,        0,        0,    26000,  0x321 },
	  /*      90 MCS-02 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          39000,
		  43300,        0,        0,        0,    39000,  0x322 },
	  /*      91 MCS-03 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          52000,
		  57800,        0,        0,        0,    52000,  0x323 },
	  /*      92 MCS-04 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          78000,
		  86700,        0,        0,        0,    78000,  0x324 },
	  /*      93 MCS-05 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         104000,
		  115600,        0,        0,        0,   104000,  0x325 },
	  /*      94 MCS-06 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         117000,
		  130000,        0,        0,        0,   117000,  0x326 },
	  /*      95 MCS-07 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         130000,
		  144400,        0,        0,        0,   130000,  0x327 },
	  /*      96 MCS-08 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         156000,
		  173300,        0,        0,        0,   156000,  0x328 },
	  /*      97 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         173000,
		  192000,        0,        0,        0,   173000,  0x329 },
	  /*	 98 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  195000,	 216700,	    0,		 0,	   0,
		  195000,  0x32a },
	  /*	 99 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  216700,	 240700,	    0,		 0,	   0,
		  216700,  0x32b    },

	  /* when number of spatial streams > 2 */
	  /*     100 MCS-00 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          19500,
		  21700,        0,        0,        0,    19500,  0x340 },
	  /*     101 MCS-01 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          39000,
		  43300,        0,        0,        0,    39000,  0x341 },
	  /*     102 MCS-02 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          58500,
		  65000,        0,        0,        0,    58500,  0x342 },
	  /*     103 MCS-03 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          78000,
		  86700,        0,        0,        0,    78000,  0x343 },
	  /*     104 MCS-04 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         117000,
		  130000,        0,        0,        0,   117000,  0x344 },
	  /*     105 MCS-05 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         156000,
		  173300,        0,        0,        0,   156000,  0x345 },
	  /*     106 MCS-06 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         175500,
		  195000,        0,        0,        0,   175500,  0x346 },
	  /*     107 MCS-07 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         195000,
		  216700,        0,        0,        0,   195000,  0x347 },
	  /*     108 MCS-08 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         234000,
		  260000,        0,        0,        0,   234000,  0x348 },
	  /*     109 MCS-09 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         260000,
		  288900,        0,        0,        0,   260000,  0x349 },
	  /*	    110 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  292500,	 325000,	    0,		  0,	    0,
		  292500,  0x34a},
	  /*	    111 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  325000,	 361100,	    0,		  0,	    0,
		  325000,  0x34b},

	  /* when number of spatial streams > 3 */
	  /*     112 MCS-00 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          26000,
		  28900,        0,        0,        0,    26000,  0x360 },
	  /*     113 MCS-01 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          52000,
		  57800,        0,        0,        0,    52000,  0x361 },
	  /*     114 MCS-02 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          78000,
		  86700,        0,        0,        0,    78000,  0x362 },
	  /*     115 MCS-03 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         104000,
		  115600,        0,        0,        0,   104000,  0x363 },
	  /*     116 MCS-04 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         156000,
		  173300,        0,        0,        0,   156000,  0x364 },
	  /*     117 MCS-05 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         208000,
		  231100,        0,        0,        0,   208000,  0x365 },
	  /*     118 MCS-06 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         234000,
		  260000,        0,        0,        0,   234000,  0x366 },
	  /*     119 MCS-07 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         260000,
		  288900,        0,        0,        0,   260000,  0x367 },
	  /*     120 MCS-08 */ { VHT20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         312000,
		  346700,        0,        0,        0,   312000,  0x368 },
	  /*     121 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         344000,
		  378400,        0,        0,        0,   344000,  0x369 },
	  /*    122 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  390000,   433300,        0,     0, 0,    390000,
		  0x36a},
	  /*     123 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                       433300,
		  481500,        0,	      0,	0,    433300,  0x36b},

	  /* when number of spatial streams > 4 */
	  /*     124 MCS-00 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          32500,
		  36100,        0,        0,        0,    32500,  0x380 },
	  /*     125 MCS-01 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          65000,
		  72200,        0,        0,        0,    65000,  0x381 },
	  /*     126 MCS-02 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          97500,
		  108300,        0,        0,        0,    97500,  0x382 },
	  /*     127 MCS-03 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         130000,
		  144400,        0,        0,        0,   130000,  0x383 },
	  /*     128 MCS-04 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         195000,
		  216700,        0,        0,        0,   195000,  0x384 },
	  /*     129 MCS-05 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         260000,
		  288900,        0,        0,        0,   260000,  0x385 },
	  /*     130 MCS-06 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         292500,
		  325000,        0,        0,        0,   292500,  0x386 },
	  /*     131 MCS-07 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         325000,
		  361100,        0,        0,        0,   325000,  0x387 },
	  /*     132 MCS-08 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         390000,
		  433300,        0,        0,        0,   390000,  0x388 },
	  /*     133 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         433300,
		  481500,        0,        0,        0,   433300,  0x389 },
	  /*     134 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         487500,
		  541700,        0,        0,        0,   487500,  0x38a },
	  /*     135 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         541700,
		  601900,        0,        0,        0,   541700,  0x38b },

	  /* When number of spatial streams > 5 */
	  /*     136 MCS-00 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          39000,
		  43300,        0,        0,        0,    39000,  0x3a0 },
	  /*     137 MCS-01 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          78000,
		  86700,        0,        0,        0,    78000,  0x3a1 },
	  /*     138 MCS-02 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         117000,
		  130000,        0,        0,        0,   117000,  0x3a2 },
	  /*     139 MCS-03 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         156000,
		  173300,        0,        0,        0,   156000,  0x3a3 },
	  /*     140 MCS-04 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         234000,
		  260000,        0,        0,        0,   234000,  0x3a4 },
	  /*     141 MCS-05 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         312000,
		  346700,        0,        0,        0,   312000,  0x3a5 },
	  /*     142 MCS-06 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         351000,
		  390000,        0,        0,        0,   351000,  0x3a6 },
	  /*     143 MCS-07 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         390000,
		  433300,        0,        0,        0,   390000,  0x3a7 },
	  /*     144 MCS-08 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         468000,
		  520000,        0,        0,        0,   468000,  0x3a8 },
	  /*     145 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         520000,
		  577800,        0,        0,        0,   520000,  0x3a9 },
	  /*    146 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  585000, 650000,   0, 0,   0,
		  585000,  0x3aa },
	  /*     147 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,
		  650000, 722200,	       0,		 0,
		  0, 650000,  0x3ab },

	  /* when number of spatial streams > 6 */
	  /*     148 MCS-00 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          45500,
		  50600,        0,        0,        0,    45500,  0x3c0 },
	  /*     149 MCS-01 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          91000,
		  101100,        0,        0,       0,    91000,  0x3c1 },
	  /*     150 MCS-02 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         136500,
		  151700,        0,        0,        0,   136500,  0x3c2 },
	  /*     151 MCS-03 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         182000,
		  202200,        0,        0,        0,   182000,  0x3c3 },
	  /*     152 MCS-04 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         273000,
		  303300,        0,        0,        0,   273000,  0x3c4 },
	  /*     153 MCS-05 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         364000,
		  404400,        0,        0,        0,   364000,  0x3c5 },
	  /*     154 MCS-06 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         409500,
		  455000,        0,        0,        0,   409500,  0x3c6 },
	  /*     155 MCS-07 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         455000,
		  505600,        0,        0,        0,   455000,  0x3c7 },
	  /*     156 MCS-08 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         546000,
		  606700,        0,        0,        0,   546000,  0x3c8 },
	  /*     157 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         606700,
		  674100,        0,        0,        0,   606700,  0x3c9 },
	  /*     158 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         682500,
		  758300,        0,        0,        0,   682500,  0x3ca },
	  /*     159 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         758300,
		  842600,        0,        0,        0,   758300,  0x3cb },

	  /* when number of spatial streams > 7 */
	  /*     160 MCS-00 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                          52000,
		  57800,        0,        0,        0,    52000,  0x3e0 },
	  /*     161 MCS-01 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         104000,
		  115600,        0,        0,        0,   104000,  0x3e1 },
	  /*     162 MCS-02 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         156000,
		  173300,        0,        0,        0,   156000,  0x3e2 },
	  /*     163 MCS-03 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         208000,
		  231100,        0,        0,        0,   208000,  0x3e3 },
	  /*     164 MCS-04 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         312000,
		  346700,        0,        0,        0,   312000,  0x3e4 },
	  /*     165 MCS-05 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         416000,
		  462200,        0,        0,        0,   416000,  0x3e5 },
	  /*     166 MCS-06 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         468000,
		  520000,        0,        0,        0,   468000,  0x3e6 },
	  /*     167 MCS-07 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         520000,
		  577800,        0,        0,        0,   520000,  0x3e7 },
	  /*     168 MCS-08 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         624000,
		  693300,        0,        0,        0,   624000,  0x3e8 },
	  /*     169 MCS-09 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         693300,
		  770400,        0,        0,        0,   693300,  0x3e9 },
	  /*     170 MCS-10 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         780000,
		  866700,        0,        0,        0,   780000,  0x3ea },
	  /*     171 MCS-11 */ { VHT20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_20,                         866700,
		  963000,        0,        0,        0,   866700,  0x3eb },

	  /* 11ac VHT40 rates                                                 */
	  /*     172 MCS-00 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          13500,
		  15000,        0,        0,        0,    13500,  0x300 },
	  /*     173 MCS-01 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          27000,
		  30000,        0,        0,        0,    27000,  0x301 },
	  /*     174 MCS-02 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          40500,
		  45000,        0,        0,        0,    40500,  0x302 },
	  /*     175 MCS-03 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          54000,
		  60000,        0,        0,        0,    54000,  0x303 },
	  /*     176 MCS-04 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          81000,
		  90000,        0,        0,        0,    81000,  0x304 },
	  /*     177 MCS-05 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         108000,
		  120000,        0,        0,        0,   108000,  0x305 },
	  /*     178 MCS-06 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         121500,
		  135000,        0,        0,        0,   121500,  0x306 },
	  /*     179 MCS-07 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         135000,
		  150000,        0,        0,        0,   135000,  0x307 },
	  /*     180 MCS-08 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         162000,
		  180000,        0,        0,        0,   162000,  0x308 },
	  /*     181 MCS-09 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         180000,
		  200000,        0,        0,        0,   180000,  0x309 },
	  /*     182 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         202500,
		  225000,        0,        0,        0,   202500,  0x30a },
	  /*     183 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         225000,
		  250000,        0,        0,        0,   225000,  0x30b },

	  /*  when number of spatial streams > 1 */
	  /*     184 MCS-00 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          27000,
		  30000,        0,        0,        0,    27000,  0x320 },
	  /*     185 MCS-01 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          54000,
		  60000,        0,        0,        0,    54000,  0x321 },
	  /*     186 MCS-02 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          81000,
		  90000,        0,        0,        0,    81000,  0x322 },
	  /*     187 MCS-03 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         108000,
		  120000,        0,        0,        0,   108000,  0x323 },
	  /*     188 MCS-04 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         162000,
		  180000,        0,        0,        0,   162000,  0x324 },
	  /*     189 MCS-05 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         216000,
		  240000,        0,        0,        0,   216000,  0x325 },
	  /*     190 MCS-06 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         243000,
		  270000,        0,        0,        0,   243000,  0x326 },
	  /*     191 MCS-07 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         270000,
		  300000,        0,        0,        0,   270000,  0x327 },
	  /*     192 MCS-08 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         324000,
		  360000,        0,        0,        0,   324000,  0x328 },
	  /*     193 MCS-09 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         360000,
		  400000,        0,        0,        0,   360000,  0x329 },
	  /*     194 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         405000,
		  450000,        0,        0,        0,   405000,  0x32a },
	  /*     195 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         450000,
		  500000,        0,        0,        0,   450000,  0x32b },

	  /* When number of spatial streams > 2 use below rate*/
	  /*     196 MCS-00 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          40500,
		  45000,        0,        0,        0,    40500,  0x340 },
	  /*     197 MCS-01 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          81000,
		  90000,        0,        0,        0,    81000,  0x341 },
	  /*     198 MCS-02 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         121500,
		  135000,        0,        0,        0,   121500,  0x342 },
	  /*     199 MCS-03 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         162000,
		  180000,        0,        0,        0,   162000,  0x343 },
	  /*     200 MCS-04 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         243000,
		  270000,        0,        0,        0,   243000,  0x344 },
	  /*     201 MCS-05 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         324000,
		  360000,        0,        0,        0,   324000,  0x345 },
	  /*     202 MCS-06 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         364500,
		  405000,        0,        0,        0,   364500,  0x346 },
	  /*     203 MCS-07 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         405000,
		  450000,        0,        0,        0,   405000,  0x347 },
	  /*     204 MCS-08 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         486000,
		  540000,        0,        0,        0,   486000,  0x348 },
	  /*     205 MCS-09 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         540000,
		  600000,        0,        0,        0,   540000,  0x349 },
	  /*     206 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         607500,
		  675000,        0,        0,        0,   607500,  0x34a},
	  /*     207 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         675000,
		  750000,        0,        0,        0,   675000,  0x34b},

	  /* When number of spatial streams > 3 use below rates */
	  /*     208 MCS-00 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          54000,
		  60000,        0,        0,        0,    54000,  0x360},
	  /*     209 MCS-01 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         108000,
		  120000,        0,        0,        0,   108000,  0x361},
	  /*     210 MCS-02 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         162000,
		  180000,        0,        0,        0,   162000,  0x362},
	  /*     211 MCS-03 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         216000,
		  240000,        0,        0,        0,   216000,  0x363},
	  /*     212 MCS-04 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         324000,
		  260000,        0,        0,        0,   324000,  0x364},
	  /*     213 MCS-05 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         432000,
		  480000,        0,        0,        0,   432000,  0x365},
	  /*     214 MCS-06 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         486000,
		  540000,        0,        0,        0,   486000,  0x366},
	  /*     215 MCS-07 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         540000,
		  600000,        0,        0,        0,   540000,  0x367},
	  /*     216 MCS-08 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         648000,
		  720000,        0,        0,        0,   648000,  0x368},
	  /*     217 MCS-09 */ { VHT40_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         720000,
		  800000,        0,        0,        0,   720000,  0x369},
	  /*     218 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         810000,
		  900000,        0,        0,        0,   810000,  0x36a },
	  /*     219 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         900000,
		  1000000,        0,        0,        0,   900000,  0x36b },

	  /* when number of spatial streams > 4 use below rates */
	  /*     220 MCS-00 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                          67500,
		  75000,        0,        0,        0,    67500,  0x380 },
	  /*     221 MCS-01 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         135000,
		  150000,        0,        0,        0,   135000,  0x381 },
	  /*     222 MCS-02 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         202500,
		  225000,        0,        0,        0,   202500,  0x382 },
	  /*     223 MCS-03 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         270000,
		  300000,        0,        0,        0,   270000,  0x383 },
	  /*     224 MCS-04 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         405000,
		  450000,        0,        0,        0,   405000,  0x384 },
	  /*     225 MCS-05 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         540000,
		  600000,        0,        0,        0,   540000,  0x385 },
	  /*     226 MCS-06 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         607500,
		  675000,        0,        0,        0,   607500,  0x386 },
	  /*     227 MCS-07 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,                         675000,
		  750000,        0,        0,        0,   675000,  0x387 },
	  /*     228 MCS-08 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  810000, 900000,        0,        0,        0,   810000,
		  0x388 },
	  /*     229 MCS-09 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  900000, 1000000,        0,        0,        0,   900000,
		  0x389 },
	  /*     230 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1012500, 1125000,        0,        0,        0,   1012500,
		  0x38a },
	  /*     231 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1125000, 1250000,        0,        0,        0,   1125000,
		  0x38b },

	  /* when number of spatial streams > 5 use below rates*/
	  /*     232 MCS-00 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  81000, 90000,        0,        0,        0,    81000,  0x3a0
	  },
	  /*     233 MCS-01 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  162000, 180000,        0,        0,        0,   162000,
		  0x3a1 },
	  /*     234 MCS-02 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  243000, 270000,        0,        0,        0,   243000,
		  0x3a2 },
	  /*     235 MCS-03 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  324000, 360000,        0,        0,        0,   324000,
		  0x3a3 },
	  /*     236 MCS-04 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  486000, 540000,        0,        0,        0,   486000,
		  0x3a4 },
	  /*     237 MCS-05 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  648000, 720000,        0,        0,        0,   648000,
		  0x3a5 },
	  /*     238 MCS-06 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  729000, 815000,        0,        0,        0,   729000,
		  0x3a6 },
	  /*     239 MCS-07 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  810000, 900000,        0,        0,        0,   810000,
		  0x3a7 },
	  /*     240 MCS-08 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  972000, 1080000,        0,        0,        0,   972000,
		  0x3a8 },
	  /*     241 MCS-09 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1080000, 1200000,        0,        0,        0,  1080000,
		  0x3a9 },
	  /*     242 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1215000, 1350000,        0,        0,        0,  1215000,
		  0x3aa },
	  /*     243 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1350000, 1500000,        0,        0,        0,  1350000,
		  0x3ab },

	  /* when number of spatial streams > 6 use below rates */
	  /*     244 MCS-00 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  94500, 105000,        0,        0,        0,    94500,  0x3c0
	  },
	  /*     245 MCS-01 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  189000, 210000,        0,        0,        0,   189000,
		  0x3c1 },
	  /*     246 MCS-02 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  283500, 315000,        0,        0,        0,   283500,
		  0x3c2 },
	  /*     247 MCS-03 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  378000, 420000,        0,        0,        0,   378000,
		  0x3c3 },
	  /*     248 MCS-04 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  567000, 630000,        0,        0,        0,   567000,
		  0x3c4 },
	  /*     249 MCS-05 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  756000, 840000,        0,        0,        0,   756000,
		  0x3c5 },
	  /*     250 MCS-06 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  850500, 945000,        0,        0,        0,   850500,
		  0x3c6 },
	  /*     251 MCS-07 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  945000, 1050000,        0,        0,        0,   945000,
		  0x3c7 },
	  /*     252 MCS-08 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1134000, 1260000,        0,        0,        0,  1134000,
		  0x3c8 },
	  /*     253 MCS-09 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1260000, 1400000,        0,        0,        0,  1260000,
		  0x3c9 },
	  /*     254 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1417500, 1575000,        0,        0,        0,  1417500,
		  0x3ca },
	  /*     255 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1575000, 1750000,        0,        0,        0,  1575000,
		  0x3cb },

	  /* when number of spatial streams > 7 use below rates */
	  /*     256 MCS-00 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  108000, 120000,        0,        0,        0,   108000,
		  0x3e0 },
	  /*     257 MCS-01 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  216000, 240000,        0,        0,        0,   216000,
		  0x3e1 },
	  /*     258 MCS-02 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  324000, 360000,        0,        0,        0,   324000,
		  0x3e2 },
	  /*     259 MCS-03 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  432000, 480000,        0,        0,        0,   432000,
		  0x3e3 },
	  /*     260 MCS-04 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  648000, 720000,        0,        0,        0,   648000,
		  0x3e4 },
	  /*     261 MCS-05 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  864000, 960000,        0,        0,        0,   864000,
		  0x3e5 },
	  /*     262 MCS-06 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  972000, 1080000,        0,        0,        0,   972000,
		  0x3e6 },
	  /*     263 MCS-07 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1080000, 1200000,        0,        0,        0,  1080000,
		  0x3e7 },
	  /*     264 MCS-08 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1296000, 1440000,        0,        0,        0,  1296000,
		  0x3e8 },
	  /*     265 MCS-09 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1440000, 1600000,        0,        0,        0,  1440000,
		  0x3e9 },
	  /*     266 MCS-10 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1620000, 1800000,        0,        0,        0,  1620000,
		  0x3ea },
	  /*     267 MCS-11 */ { VHT40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_40,
		  1800000, 2000000,        0,        0,        0,  1800000,
		  0x3eb },

	  /* 11ac VHT80 rates
	   */
	  /*     268 MCS-00 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  29300, 32500,        0,        0,        0,    29300,
		  0x300},
	  /*     269 MCS-01 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  58500, 65000,        0,        0,        0,    58500,
		  0x301},
	  /*     270 MCS-02 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  87800, 97500,        0,        0,        0,    87800,
		  0x302},
	  /*     271 MCS-03 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  117000, 130000,        0,        0,        0,   117000,
		  0x303},
	  /*     272 MCS-04 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  175500, 195000,        0,        0,        0,   175500,
		  0x304},
	  /*     273 MCS-05 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  234000, 260000,        0,        0,        0,   234000,
		  0x305},
	  /*     274 MCS-06 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  263300, 292500,        0,        0,        0,   263300,
		  0x306},
	  /*     275 MCS-07 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  292500, 325000,        0,        0,        0,   292500,
		  0x307},
	  /*     276 MCS-08 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  351000, 390000,        0,        0,        0,   351000,
		  0x308},
	  /*     277 MCS-09 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  390000, 433300,        0,        0,        0,   390000,
		  0x309},
	  /*     278 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  438800, 487500,        0,        0,        0,   438800,
		  0x30a},
	  /*     279 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  487500, 541700,        0,        0,        0,   487500,
		  0x30b},

	  /* When number of spatial streams > 1 use below rates*/
	  /*     280 MCS-00 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  58500, 65000,        0,        0,        0,    58500,
		  0x320},
	  /*     281 MCS-01 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  117000, 130000,        0,        0,        0,   117000,
		  0x321},
	  /*     282 MCS-02 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  175500, 195000,        0,        0,        0,   175500,
		  0x322},
	  /*     283 MCS-03 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  234000, 260000,        0,        0,        0,   234000,
		  0x323},
	  /*     284 MCS-04 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  351000, 390000,        0,        0,        0,   351000,
		  0x324},
	  /*     285 MCS-05 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  468000, 520000,        0,        0,        0,   468000,
		  0x325},
	  /*     286 MCS-06 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  526500, 585000,        0,        0,        0,   526500,
		  0x326},
	  /*     287 MCS-07 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  585000, 650000,        0,        0,        0,   585000,
		  0x327},
	  /*     288 MCS-08 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  702000, 780000,        0,        0,        0,   702000,
		  0x328},
	  /*     289 MCS-09 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  780000, 866700,        0,        0,        0,   780000,
		  0x329},
	  /*     290 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  877500, 975000,        0,        0,        0,   877500,
		  0x32a},
	  /*     291 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  975000, 1083300,        0,        0,        0,   975000,
		  0x32b},

	  /* When number of spatial streams > 2 use below rates */
	  /*     292 MCS-00 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  87800, 97500,        0,        0,        0,    87800,  0x340
	  },
	  /*     293 MCS-01 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  175500, 195000,        0,        0,        0,   175500,
		  0x341 },
	  /*     294 MCS-02 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  263300, 292500,        0,        0,        0,   263300,
		  0x342 },
	  /*     295 MCS-03 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  351000, 390000,        0,        0,        0,   351000,
		  0x343 },
	  /*     296 MCS-04 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  526500, 585000,        0,        0,        0,   526500,
		  0x344 },
	  /*     297 MCS-05 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  702000, 780000,        0,        0,        0,   702000,
		  0x345 },
	  /*     298 MCS-06 */ { VHT_INVALID_RATES_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  789800, 877500,        0,        0,        0,   789800,
		  0x346 },
	  /*     299 MCS-07 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  877500, 975000,        0,        0,        0,   877500,
		  0x347 },
	  /*     300 MCS-08 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1053000, 1170000,        0,        0,        0,  1053000,
		  0x348 },
	  /*     301 MCS-09 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1170000, 1300000,        0,        0,        0,  1170000,
		  0x349 },
	  /*     302 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1316300, 1462500,        0,        0,        0,  1316300,
		  0x34a },
	  /*     303 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1462500, 1625000,        0,        0,        0,  1462500,
		  0x34b },
	  /* When number of spatial streams > 3 use below rates */
	  /*     304 MCS-00 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  117000, 130000,        0,        0,        0,   117000,
		  0x360 },
	  /*     305 MCS-01 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  234000, 260000,        0,        0,        0,   234000,
		  0x361 },
	  /*     306 MCS-02 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  351000, 390000,        0,        0,        0,   351000,
		  0x362 },
	  /*     307 MCS-03 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  468000, 520000,        0,        0,        0,   468000,
		  0x363 },
	  /*     308 MCS-04 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  702000, 780000,        0,        0,        0,   702000,
		  0x364 },
	  /*     309 MCS-05 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  936000, 1040000,        0,        0,        0,   936000,
		  0x365 },
	  /*     310 MCS-06 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1053000, 1170000,        0,        0,        0,  1053000,
		  0x366 },
	  /*     311 MCS-07 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1170000, 1300000,        0,        0,        0,  1170000,
		  0x367 },
	  /*     312 MCS-08 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1404000, 1560000,        0,        0,        0,  1404000,
		  0x368 },
	  /*     313 MCS-09 */ { VHT80_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1560000, 1733000,        0,        0,        0,  1560000,
		  0x369 },
	  /*     314 MCS-08 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1755000, 1950000,        0,        0,        0,  1755000,
		  0x36a },
	  /*     315 MCS-09 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1950000, 2166700,        0,        0,        0,  1950000,
		  0x36b },
	  /* When number of spatial streams > 4 use below rates */
	  /*     316 MCS-00 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  146300, 162500,        0,        0,        0,   146300,
		  0x380 },
	  /*     317 MCS-01 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  292500, 325000,        0,        0,        0,   292500,
		  0x381 },
	  /*     318 MCS-02 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  438800, 487500,        0,        0,        0,   438800,
		  0x382 },
	  /*     319 MCS-03 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  585000, 650000,        0,        0,        0,   585000,
		  0x383 },
	  /*     320 MCS-04 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  877500, 975000,        0,        0,        0,   877500,
		  0x384 },
	  /*     321 MCS-05 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1170000, 1300000,        0,        0,        0,  1170000,
		  0x385 },
	  /*     322 MCS-06 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1316300, 1462500,        0,        0,        0,  1316300,
		  0x386 },
	  /*     323 MCS-07 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1462500, 1625000,        0,        0,        0,  1462500,
		  0x387 },
	  /*     324 MCS-08 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1755000, 1950000,        0,        0,        0,  1755000,
		  0x388 },
	  /*     325 MCS-09 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1950000, 2166700,        0,        0,        0,  1950000,
		  0x389 },
	  /*     326 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2193800, 2437500,        0,        0,        0,  2193800,
		  0x38a },
	  /*     327 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2437500, 2708300,        0,        0,        0,  2437500,
		  0x38b },
	  /* When number of spatial streams > 5 use below rates */
	  /*     328 MCS-00 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  175500, 195000,        0,        0,        0,   175500,
		  0x3a0 },
	  /*     329 MCS-01 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  351000, 390000,        0,        0,        0,   351000,
		  0x3a1 },
	  /*     330 MCS-02 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  526500, 585500,        0,        0,        0,   526500,
		  0x3a2 },
	  /*     331 MCS-03 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  702000, 780000,        0,        0,        0,   702000,
		  0x3a3 },
	  /*     332 MCS-04 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1053000, 1170000,        0,        0,        0,  1053000,
		  0x3a4 },
	  /*     333 MCS-05 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1404000, 1560000,        0,        0,        0,  1404000,
		  0x3a5 },
	  /*     334 MCS-06 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1579500, 1755000,        0,        0,        0,  1579500,
		  0x3a6 },
	  /*     335 MCS-07 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1755000, 1950000,        0,        0,        0,  1755000,
		  0x3a7 },
	  /*     336 MCS-08 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2106000, 2340000,        0,        0,        0,  2106000,
		  0x3a8 },
	  /*     337 MCS-09 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2340000, 2600000,        0,        0,        0,  2340000,
		  0x3a9 },
	  /*     338 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2632500, 2925000,        0,        0,        0,  2632500,
		  0x3aa },
	  /*     339 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2925000, 3250000,        0,        0,        0,  2925000,
		  0x3ab },
	  /* When number of spatial streams > 6 use below rates*/
	  /*     340 MCS-00 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  204800, 227500,        0,        0,        0,   204800,
		  0x3c0 },
	  /*     341 MCS-01 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  409500, 455000,        0,        0,        0,   409500,
		  0x3c1 },
	  /*     342 MCS-02 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  614300, 682500,        0,        0,        0,   614300,
		  0x3c2 },
	  /*     343 MCS-03 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  819000, 910000,        0,        0,        0,   819000,
		  0x3c3 },
	  /*     344 MCS-04 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1288500, 1365000,        0,        0,        0,  1288500,
		  0x3c4 },
	  /*     345 MCS-05 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1638000, 1820000,        0,        0,        0,  1638000,
		  0x3c5 },
	  /*     346 MCS-06 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1842800, 2047500,        0,        0,        0,  1842800,
		  0x3c6 },
	  /*     347 MCS-07 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2047500, 2275000,        0,        0,        0,  2047500,
		  0x3c7 },
	  /*     348 MCS-08 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2457000, 2730000,        0,        0,        0,  2457000,
		  0x3c8 },
	  /*     349 MCS-09 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2730000, 3033300,        0,        0,        0,  2730000,
		  0x3c9 },
	  /*     350 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  3071300, 3412500,        0,        0,        0,  3071300,
		  0x3ca },
	  /*     351 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  3412500, 3791700,        0,        0,        0,  3412500,
		  0x3cb },
	  /* When number of spatial streams > 7 use below rates*/
	  /*     352 MCS-00 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  234000, 260000,        0,        0,        0,   234000,
		  0x3e0 },
	  /*     353 MCS-01 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  468000, 520000,        0,        0,        0,   468000,
		  0x3e1},
	  /*     354 MCS-02 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  702000, 780000,        0,        0,        0,   702000,
		  0x3e2},
	  /*     355 MCS-03 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  936000, 1040000,        0,        0,        0,   936000,
		  0x3e3},
	  /*     356 MCS-04 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1404000, 1560000,        0,        0,        0,  1404000,
		  0x3e4},
	  /*     357 MCS-05 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  1872000, 2080000,        0,        0,        0,  1872000,
		  0x3e5},
	  /*     358 MCS-06 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2106000, 2340000,        0,        0,        0,  2106000,
		  0x3e6},
	  /*     359 MCS-07 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2340000, 2600000,        0,        0,        0,  2340000,
		  0x3e7},
	  /*     360 MCS-08 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  2808000, 3120000,        0,        0,        0,  2808000,
		  0x3e8},
	  /*     361 MCS-09 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  3120000, 3466700,        0,        0,        0,  3120000,
		  0x3e9},
	  /*     362 MCS-10 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  3510000, 3900000,        0,        0,        0,  3510000,
		  0x3ea},
	  /*     363 MCS-11 */ { VHT80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_80,
		  3900000, 4333300,        0,        0,        0,  3900000,
		  0x3eb},

	  /* 11ac VHT160 rates
	   */
	  /*     364 MCS-00 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  58500, 65000,        0,        0,        0,    58500,
		  0x300},
	  /*     365 MCS-01 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  117000, 130000,        0,        0,        0,   117000,
		  0x301},
	  /*     366 MCS-02 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  175500, 195000,        0,        0,        0,   175500,
		  0x302},
	  /*     367 MCS-03 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  234000, 260000,        0,        0,        0,   234000,
		  0x303},
	  /*     368 MCS-04 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  351000, 390000,        0,        0,        0,   351000,
		  0x304},
	  /*     369 MCS-05 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  468000, 520000,        0,        0,        0,   468000,
		  0x305},
	  /*     370 MCS-06 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  526500, 585000,        0,        0,        0,   526500,
		  0x306},
	  /*     371 MCS-07 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  585000, 650000,        0,        0,        0,   585000,
		  0x307},
	  /*     372 MCS-08 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  702000, 780000,        0,        0,        0,   702000,
		  0x308},
	  /*     373 MCS-09 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  780000, 866700,        0,        0,        0,   780000,
		  0x309},
	  /*     374 MCS-10 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  877500, 975000,        0,        0,        0,   877500,
		  0x30a },
	  /*     375 MCS-11 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  975000, 1083300,        0,        0,        0,   975000,
		  0x30b },
	  /* If maximum number of spatial streams supported
	   * at 160MHZ > 1 use below rates
	   */
	  /*     376 MCS-00 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  117000, 130000,        0,        0,        0,   117000,
		  0x320},
	  /*     377 MCS-01 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  234000, 260000,        0,        0,        0,   234000,
		  0x321},
	  /*     378 MCS-02 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  351000, 390000,        0,        0,        0,   351000,
		  0x322},
	  /*     379 MCS-03 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  468000, 520000,        0,        0,        0,   468000,
		  0x323},
	  /*     380 MCS-04 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  702000, 780000,        0,        0,        0,   702000,
		  0x324},
	  /*     381 MCS-05 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  936000, 1040000,        0,        0,        0,   936000,
		  0x325},
	  /*     382 MCS-06 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1053000, 1170000,        0,        0,        0,  1053000,
		  0x326},
	  /*     383 MCS-07 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1170000, 1300000,        0,        0,        0,  1170000,
		  0x327},
	  /*     384 MCS-08 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1404000, 1560000,        0,        0,        0,  1404000,
		  0x328},
	  /*     385 MCS-09 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1560000, 1733300,        0,        0,        0,  1560000,
		  0x329},
	  /*     386 MCS-10 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1755000, 1950000,        0,        0,        0,  1755000,
		  0x32a},
	  /*     387 MCS-11 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1950000, 2166700,        0,        0,        0,  1950000,
		  0x32b},
	  /* If maximum number of spatial streams supported
	   * at 160MHZ > 2 use below rates
	   */
	  /*     388 MCS-00 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  175500, 195000,        0,        0,        0,   175500,
		  0x340 },
	  /*     389 MCS-01 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  351000, 390000,        0,        0,        0,   351000,
		  0x341 },
	  /*     390 MCS-02 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  526500, 585000,        0,        0,        0,   526500,
		  0x342 },
	  /*     391 MCS-03 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  702000, 780000,        0,        0,        0,   702000,
		  0x343 },
	  /*     392 MCS-04 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1053000, 1170000,        0,        0,        0,  1053000,
		  0x344 },
	  /*     393 MCS-05 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1404000, 1560000,        0,        0,        0,  1404000,
		  0x345 },
	  /*     394 MCS-06 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1579500, 1755000,        0,        0,        0,  1579500,
		  0x346 },
	  /*     395 MCS-07 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1755000, 1755000,        0,        0,        0,  1755000,
		  0x347 },
	  /*     396 MCS-08 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2106000, 2340000,        0,        0,        0,  2106000,
		  0x348 },
	  /*     397 MCS-09 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2340000, 2600000,        0,        0,        0,  2340000,
		  0x349 },
	  /*     398 MCS-10 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2632500, 2925000,        0,        0,        0,  2632500,
		  0x34a },
	  /*     399 MCS-11 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2925000, 3250000,        0,        0,        0,  2925000,
		  0x34b },
	  /* If maximum number of spatial streams supported
	   * at 160MHZ > 3 use below rates
	   */
	  /*     400 MCS-00 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  234000, 260000,        0,        0,        0,   234000,
		  0x360 },
	  /*     401 MCS-01 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  468000, 520000,        0,        0,        0,   468000,
		  0x361 },
	  /*     402 MCS-02 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  702000, 780000,        0,        0,        0,   702000,
		  0x362 },
	  /*     403 MCS-03 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  936000, 1040000,        0,        0,        0,   936000,
		  0x363 },
	  /*     404 MCS-04 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1404000, 1560000,        0,        0,        0,  1404000,
		  0x364 },
	  /*     405 MCS-05 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  1872000, 2080000,        0,        0,        0,  1872000,
		  0x365 },
	  /*     406 MCS-06 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2106000, 2340000,        0,        0,        0,  2106000,
		  0x366 },
	  /*     407 MCS-07 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2340000, 2600000,        0,        0,        0,  2340000,
		  0x367 },
	  /*     408 MCS-08 */ { VHT160_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  2808000, 3120000,        0,        0,        0,  2808000,
		  0x368 },
	  /*     409 MCS-09 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  3120000, 3466700,        0,        0,        0,  3120000,
		  0x369 },
	  /*     410 MCS-10 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  3510000, 3900000,        0,        0,        0,  3510000,
		  0x36a },
	  /*     411 MCS-11 */ { VHT160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_VHT_160,
		  3900000, 4333300,        0,        0,        0,  3900000,
		  0x36b },

	  /* 11ax RU242 rates
	   */
	  /*     412 MCS-00 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  8600, 8900,     8100,     7300,     4300,     8600,  0x400},
	  /*     413 MCS-01 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  17200, 17700,    16300,    14600,     8600,    17200,
		  0x401},
	  /*     414 MCS-02 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  25800, 26600,    24400,    21900,        0,    25800,
		  0x402},
	  /*     415 MCS-03 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  34400, 35500,    32500,    29300,    17700,    34400,
		  0x403},
	  /*     416 MCS-04 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  51600, 53200,    48800,    43900,    25800,    51600,
		  0x404},
	  /*     417 MCS-05 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  68800, 70900,    65000,    58500,        0,    68800,
		  0x405},
	  /*     418 MCS-06 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  77400, 79800,    73100,    65800,        0,    77400,
		  0x406},
	  /*     419 MCS-07 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  86000, 88600,    81300,    73100,        0,    86000,
		  0x407},
	  /*     420 MCS-08 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  103200, 106400,    97500,    87800,        0,   103200,
		  0x408},
	  /*     421 MCS-09 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  114700, 118200,   108300,    97500,        0,   114700,
		  0x409},
	  /*     422 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  129000, 133000,   121900,   109700,        0,   129000,
		  0x40a},
	  /*     423 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  143400, 147700,   135400,   121900,        0,   143400,
		  0x40b},
	  /*     424 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  154900, 159500,   146300,   131600,        0,   154900,
		  0x40c},
	  /*     425 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  172100, 177300,   162500,   146300,        0,   172100,
		  0x40d},
	  /* When number spatial streams > 1 use below rates */
	  /*     426 MCS-00 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  17200, 17700,    16300,    14600,     8600,    17200,
		  0x420},
	  /*     427 MCS-01 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  34400, 35500,    32500,    29300,    17700,    34400,
		  0x421},
	  /*     428 MCS-02 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  51600, 53200,    48800,    43900,        0,    51600,
		  0x422},
	  /*     429 MCS-03 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  68800, 70900,    65000,    58500,    34400,    68800,
		  0x423},
	  /*     430 MCS-04 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  103200, 106400,    97500,    87800,    51600,   103200,
		  0x424},
	  /*     431 MCS-05 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  137600, 141800,   130000,   117000,        0,   137600,
		  0x425},
	  /*     432 MCS-06 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  154900, 159500,   146300,   131600,        0,   154900,
		  0x426},
	  /*     433 MCS-07 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  172100, 177300,   162500,   146300,        0,   172100,
		  0x427},
	  /*     434 MCS-08 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  206500, 212700,   195000,   175500,        0,   206500,
		  0x428},
	  /*     435 MCS-09 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  229400, 236400,   216700,   195000,        0,   229400,
		  0x429},
	  /*     436 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  258100, 265900,   243800,   219400,        0,   258100,
		  0x42a},
	  /*     437 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  286800, 295500,   270800,   243800,        0,   286800,
		  0x42b},
	  /*     438 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  309700, 319100,   292500,   263300,        0,   309700,
		  0x42c},
	  /*     439 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  344100, 354500,   325000,   292500,        0,   344100,
		  0x42d},

	  /* When number of spatial streams > 2
	   * use below rates
	   */
	  /*     440 MCS-00 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  25800, 26600,    24400,    21900,    12900,    25800,
		  0x440},
	  /*     441 MCS-01 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  51600, 53200,    48800,    43900,    25800,    51600,
		  0x441},
	  /*     442 MCS-02 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  77400, 79800,    73100,    65800,        0,    77400,
		  0x442},
	  /*     443 MCS-03 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  103200, 106400,    97500,    87800,    51600,   103200,
		  0x443},
	  /*     444 MCS-04 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  154900, 159500,   146300,   131600,    77400,   154900,
		  0x444},
	  /*     445 MCS-05 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  206500, 212700,   195000,   175500,        0,   206500,
		  0x445},
	  /*     446 MCS-06 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  232300, 239300,   219400,   197400,        0,   232300,
		  0x446},
	  /*     447 MCS-07 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  258100, 265900,   243800,   219400,        0,   258100,
		  0x447},
	  /*     448 MCS-08 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  309700, 319100,   292500,   263300,        0,   309700,
		  0x448},
	  /*     449 MCS-09 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  344100, 354500,   325000,   292500,        0,   344100,
		  0x449},
	  /*     450 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  387100, 398900,   365600,   329100,        0,   387100,
		  0x44a},
	  /*     451 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  430100, 443200,   406300,   365600,        0,   430100,
		  0x44b},
	  /*     452 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  464600, 478600,   438800,   394900,        0,   464600,
		  0x44c},
	  /*     453 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  516200, 531800,   487500,   438800,        0,   516200,
		  0x44d},

	  /* When number of spatial streams > 3
	   * use below rates
	   */
	  /*     454 MCS-00 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  34400, 35500,    32500,    29300,    17700,    34400,
		  0x460},
	  /*     455 MCS-01 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  68800, 70900,    65000,    58500,    34400,    68800,
		  0x461},
	  /*     456 MCS-02 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  103200, 106400,    97500,    87800,        0,   103200,
		  0x462},
	  /*     457 MCS-03 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  137600, 141800,   130000,   117000,    68800,   137600,
		  0x463},
	  /*     458 MCS-04 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  206500, 212700,   195000,   175500,   103200,   206500,
		  0x464},
	  /*     459 MCS-05 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  275300, 283600,   260000,   234000,        0,   275300,
		  0x465},
	  /*     460 MCS-06 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  309700, 319100,   292500,   263300,        0,   309700,
		  0x466},
	  /*     461 MCS-07 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  344100, 354500,   325000,   292500,        0,   344100,
		  0x467},
	  /*     462 MCS-08 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  412900, 425500,   390000,   351000,        0,   412900,
		  0x468},
	  /*     463 MCS-09 */ { HE20_MODE_VALID_MASK,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  455800, 472700,   433300,   390000,        0,   455800,
		  0x469},
	  /*     464 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  516200, 531800,   487500,   438800,        0,   516200,
		  0x46a},
	  /*     465 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  573500, 590900,   541700,   487500,        0,   573500,
		  0x46b},
	  /*     466 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  619400, 638200,   585000,   526500,        0,   619400,
		  0x46c},
	  /*     467 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x46d},

	  /* When number of spatial streams > 4
	   * use below rates
	   */
	  /*     468 MCS-00 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  43000, 43300,    40600,    36600,    21500,    43000,
		  0x480},
	  /*     469 MCS-01 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  86000, 88600,    81300,    73100,    43000,    86000,
		  0x481},
	  /*     470 MCS-02 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  129000, 133000,   121900,   109700,        0,   129000,
		  0x482},
	  /*     471 MCS-03 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  172100, 177300,   162500,   146300,    86000,   172100,
		  0x483},
	  /*     472 MCS-04 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  258100, 265900,   243800,   219400,   129000,   258100,
		  0x484},
	  /*     473 MCS-05 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  344100, 354500,   325000,   292500,        0,   344100,
		  0x485},
	  /*     474 MCS-06 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  387100, 398900,   365600,   329100,        0,   387100,
		  0x486},
	  /*     475 MCS-07 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  430100, 443200,   406300,   365600,        0,   430100,
		  0x487},
	  /*     476 MCS-08 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  516200, 531800,   487500,   438800,        0,   516200,
		  0x488},
	  /*     477 MCS-09 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  573500, 590900,   541700,   487500,        0,   573500,
		  0x489},
	  /*     478 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  645200, 664800,   609400,   548400,        0,   645200,
		  0x48a},
	  /*     479 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  716900, 738600,   677100,   609400,        0,   716900,
		  0x48b},
	  /*     480 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  774300, 797700,   731300,   658100,        0,   774300,
		  0x48c},
	  /*     481 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  860300, 886400,   812500,   731300,        0,   860300,
		  0x48d},

	  /* When number of spatial streams > 5
	   * use below rates
	   */
	  /*     482 MCS-00 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  51600, 53200,    48800,    43900,    25800,    51600,
		  0x4a0},
	  /*     483 MCS-01 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  103200, 106400,    97500,    87800,    51600,   103200,
		  0x4a1},
	  /*     484 MCS-02 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  154900, 159500,   146300,   131600,        0,   154900,
		  0x4a2},
	  /*     485 MCS-03 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  206500, 212700,   195000,   175500,   103200,   206500,
		  0x4a3},
	  /*     486 MCS-04 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  309700, 319100,   292500,   263300,   154900,   309700,
		  0x4a4},
	  /*     487 MCS-05 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  412900, 425500,   390000,   351000,        0,   412900,
		  0x4a5},
	  /*     488 MCS-06 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  464600, 478600,   438000,   394900,        0,   464600,
		  0x4a6},
	  /*     489 MCS-07 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  516200, 531800,   487500,   438800,        0,   516200,
		  0x4a7},
	  /*     490 MCS-08 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  619400, 638200,   585000,   526500,        0,   619400,
		  0x4a8},
	  /*     491 MCS-09 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x4a9},
	  /*     492 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  774300, 797700,   731300,   658100,        0,   774300,
		  0x4aa},
	  /*     493 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  860300, 886400,   812500,   731300,        0,   860300,
		  0x4ab},
	  /*     494 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  929100, 957300,   877500,   789800,        0,   929100,
		  0x4ac},
	  /*     495 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1032400, 1063600,   975000,   877500,      0,   1032400,
		  0x4ad},

	  /* When number of spatial streams > 6
	   * use below rates
	   */
	  /*     496 MCS-00 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  60200, 62000,    56900,    51200,    30100,    60200,
		  0x4c0},
	  /*     497 MCS-01 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  120400, 124100,   113800,   102400,    60200,   120400,
		  0x4c1},
	  /*     498 MCS-02 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  180700, 186100,   170600,   153600,        0,   180700,
		  0x4c2},
	  /*     499 MCS-03 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  240900, 248200,   227500,   204800,   120400,   240900,
		  0x4c3},
	  /*     500 MCS-04 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  361300, 372300,   341300,   307100,   180700,   361300,
		  0x4c4},
	  /*     501 MCS-05 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  481800, 496400,   455000,   409500,        0,   481800,
		  0x4c5},
	  /*     502 MCS-06 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  542000, 558400,   511900,   460700,        0,   542000,
		  0x4c6},
	  /*     503 MCS-07 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  602200, 620500,   568800,   511900,        0,   602200,
		  0x4c7},
	  /*     504 MCS-08 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  722600, 744500,   682500,   614300,        0,   722600,
		  0x4c8},
	  /*     505 MCS-09 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  802900, 827300,   758300,   682500,        0,   802900,
		  0x4c9},
	  /*     506 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  903300, 930700,   853100,   767800,        0,   903300,
		  0x4ca},
	  /*     507 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1003700, 1034100,   947900,   853100,        0,  1003700,
		  0x4cb},
	  /*     508 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1084000, 1116800,   1023800,  921400,        0,  1084000,
		  0x4cc},
	  /*     509 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1204400, 1240900,   1137500,   1023800,       0,  1204400,
		  0x4cd},

	  /* When number of spatial streams > 7
	   * use below rates
	   */
	  /*     510 MCS-00 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  68800, 70900,    65000,    58500,    34400,    68800,
		  0x4e0},
	  /*     511 MCS-01 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  137600, 141800,   130000,   117000,    68800,   137600,
		  0x4e1},
	  /*     512 MCS-02 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  206500, 212700,   195000,   175500,        0,   206500,
		  0x4e2},
	  /*     513 MCS-03 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  275300, 283600,   260000,   234000,   137600,   275300,
		  0x4e3},
	  /*     514 MCS-04 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  412900, 425500,   390000,   351000,   206500,   412900,
		  0x4e4},
	  /*     515 MCS-05 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  550600, 567300,   520000,   468000,        0,   550600,
		  0x4e5},
	  /*     516 MCS-06 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  619400, 638200,   585000,   526500,        0,   619400,
		  0x4e6},
	  /*     517 MCS-07 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x4e7},
	  /*     518 MCS-08 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  825900, 850900,   780000,   702000,        0,   825900,
		  0x4e8},
	  /*     519 MCS-09 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  917600, 945500,   866700,   780000,        0,   917600,
		  0x4e9},
	  /*     520 MCS-10 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1032400, 1063600,   975000,   877500,        0,  1032400,
		  0x4ea},
	  /*     521 MCS-11 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1147100, 1181800,  1083300,   975000,        0,  1147100,
		  0x4eb},
	  /*     522 MCS-12 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1238800, 1276400,  1170000,   1053000,        0,  1238800,
		  0x4ec},
	  /*     523 MCS-13 */ { HE20_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_20,
		  1376500, 1418200,  1300000,   1170000,        0,  1376500,
		  0x4ed},

	  /* 11ax RU484 rates
	   */
	  /*     524 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  17200, 17700,    16300,    14600,     8600,    17200,  0x400
	  },
	  /*     525 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  34400, 35500,    32500,    29300,    17700,    34400,  0x401
	  },
	  /*     526 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  51600, 53200,    48800,    43900,    25800,    51600,  0x402
	  },
	  /*     527 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  68800, 70900,    65000,    58500,        0,    68800,  0x403
	  },
	  /*     528 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  103200, 106400,    97500,    87800,        0,   103200,
		  0x404 },
	  /*     529 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  137600, 141800,   130000,   117000,        0,   137600,
		  0x405 },
	  /*     530 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  154900, 159500,   146300,   131600,        0,   154900,
		  0x406 },
	  /*     531 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  172100, 177300,   162500,   146300,        0,   172100,
		  0x407 },
	  /*     532 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  206500, 212700,   195000,   175500,        0,   206500,
		  0x408 },
	  /*     533 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  229400, 236400,   216700,   195000,        0,   229400,
		  0x409 },
	  /*     534 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  258100, 265900,   243800,   219400,        0,   258100,
		  0x40a },
	  /*     535 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  286800, 295500,   270800,   243800,        0,   286800,
		  0x40b },
	  /*     536 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  309700, 319100,   292500,   263300,        0,   309700,
		  0x40c },
	  /*     537 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  344100, 354500,   325000,   292500,        0,   344100,
		  0x40d },
	  /* When number of spatial streams > 1
	   * use below rates
	   */
	  /*     538 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  34400, 35500,    32500,    29300,    17700,    34400,  0x420
	  },
	  /*     539 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  68800, 70900,    65000,    58500,    34400,    68800,  0x421
	  },
	  /*     540 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  103200, 106400,    97500,    87800,        0,   103200,
		  0x422 },
	  /*     541 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  137600, 141800,   130000,   117000,    68800,   137600,
		  0x423 },
	  /*     542 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  206500, 212700,   195000,   175500,   103200,   206500,
		  0x424 },
	  /*     543 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  275300, 283600,   260000,   234000,        0,   275300,
		  0x425 },
	  /*     544 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  309700, 319100,   292500,   263300,        0,   309700,
		  0x426 },
	  /*     545 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  344100, 354500,   325000,   292500,        0,   344100,
		  0x427 },
	  /*     546 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  412900, 425500,   390000,   351000,        0,   412900,
		  0x428 },
	  /*     547 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  455800, 472700,   433300,   390000,        0,   455800,
		  0x429 },
	  /*     548 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  516200, 531800,   487500,   438800,        0,   516200,
		  0x42a },
	  /*     549 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  573500, 590900,   541700,   487500,        0,   573500,
		  0x42b },
	  /*     550 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  619400, 638200,   585000,   526500,        0,   619400,
		  0x42c },
	  /*     551 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x42d },

	  /* When number of spatial streams > 2
	   * use below rates
	   */
	  /*     552 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  51600, 53200,    48800,    43900,    25800,    51600,  0x440
	  },
	  /*     553 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  103200, 106400,    97500,    87800,    51600,   103200,
		  0x441 },
	  /*     554 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  154900, 159500,   146300,   131600,        0,   154900,
		  0x442 },
	  /*     555 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  206500, 212700,   195000,   175500,   103200,   206500,
		  0x443 },
	  /*     556 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  309700, 319100,   292500,   263300,   154900,   309700,
		  0x444 },
	  /*     557 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  412900, 425500,   390000,   351000,        0,   412900,
		  0x445 },
	  /*     558 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  464600, 478600,   438000,   394900,        0,   464600,
		  0x446 },
	  /*     559 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  516200, 531800,   487500,   438800,        0,   516200,
		  0x447 },
	  /*     560 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  619400, 638200,   585000,   526500,        0,   619400,
		  0x448 },
	  /*     561 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x449 },
	  /*     562 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  774300, 797700,   731300,   658100,        0,   774300,
		  0x44a },
	  /*     563 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  860300, 886400,   812500,   731300,        0,   860300,
		  0x44b },
	  /*     564 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  929100, 957300,   877500,   789800,        0,   929100,
		  0x44c },
	  /*     565 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1032400, 1063600,  975000,   877500,       0,   1032400,
		  0x44d },

	  /* When number of spatial streams > 3
	   * use below rates
	   */
	  /*     566 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  68800, 70900,    65000,    58500,    34400,    68800,  0x460
	  },
	  /*     567 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  137600, 141800,   130000,   117000,    68800,   137600,
		  0x461 },
	  /*     568 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  206500, 212700,   195000,   175500,        0,   206500,
		  0x462 },
	  /*     569 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  275300, 283600,   260000,   234000,   137600,   275300,
		  0x463 },
	  /*     570 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  412900, 425500,   390000,   351000,   206500,   412900,
		  0x464 },
	  /*     571 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  550600, 567300,   520000,   468000,        0,   550600,
		  0x465 },
	  /*     572 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  619400, 638200,   585000,   526500,        0,   619400,
		  0x466 },
	  /*     573 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x467 },
	  /*     574 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  825900, 850900,   780000,   702000,        0,   825900,
		  0x468 },
	  /*     575 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  917600, 945500,   866700,   780000,        0,   917600,
		  0x469 },
	  /*     576 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1032400, 1063600,   975000,   877500,        0,  1032400,
		  0x46a },
	  /*     577 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1147100, 1181800,  1083300,   975000,        0,  1147100,
		  0x46b },
	  /*     578 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1238800, 1276400,  1170000,   1053000,        0, 1238800,
		  0x46c },
	  /*     579 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1376500, 1418200,  1300000,   1170000,        0,  1376500,
		  0x46d },

	  /* When number of spatial streams > 4
	   * use below rates
	   */
	  /*     580 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  86000, 88600,    81300,    73100,    43000,    86000,  0x480
	  },
	  /*     581 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  172100, 177300,   162500,   146300,    86000,   172100,
		  0x481 },
	  /*     582 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  258100, 265900,   243800,   219400,        0,   258100,
		  0x482 },
	  /*     583 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  344100, 354500,   325000,   292500,   172100,   344100,
		  0x483 },
	  /*     584 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  516200, 531800,   487500,   438800,   258100,   516200,
		  0x484 },
	  /*     585 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  688200, 709100,   650000,   585000,        0,   688200,
		  0x485 },
	  /*     586 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  774300, 797700,   731300,   658100,        0,   774300,
		  0x486 },
	  /*     587 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  860300, 886400,   812500,   731300,        0,   860300,
		  0x487 },
	  /*     588 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1032400, 1063600,   975000,   877500,        0,  1032400,
		  0x488 },
	  /*     589 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1147100, 1181800,  1083300,   975000,        0,  1147100,
		  0x489 },
	  /*     590 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1290400, 1329500,  1218800,  1096900,        0,  1290400,
		  0x48a },
	  /*     591 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1433800, 1477300,  1354200,  1218800,        0,  1433800,
		  0x48b },
	  /*     592 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1548500, 1595500,  1462500,  1316300,        0,  1548500,
		  0x48c },
	  /*     593 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1720600, 1772700,  1625000,  1462500,        0,  1720600,
		  0x48d },

	  /* When number of spatial streams > 5
	   * use below rates
	   */
	  /*     594 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  103200, 106400,    97500,    87800,    51600,   103200,
		  0x4a0 },
	  /*     595 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  206500, 212700,   195000,   175500,   103200,   206500,
		  0x4a1 },
	  /*     596 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  309700, 319100,   292500,   263300,        0,   309700,
		  0x4a2 },
	  /*     597 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  412900, 425500,   390000,   351000,   206500,   412900,
		  0x4a3 },
	  /*     598 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  619400, 638200,   585000,   526500,   309700,   619400,
		  0x4a4 },
	  /*     599 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  825900, 850900,   780000,   702000,        0,   825900,
		  0x4a5 },
	  /*     600 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  929100, 957300,   877500,   789800,        0,   929100,
		  0x4a6 },
	  /*     601 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1032400, 1063600,   975000,   877500,        0,  1032400,
		  0x4a7 },
	  /*     602 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1238800, 1276400,  1170000,  1053000,        0,  1238800,
		  0x4a8 },
	  /*     603 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1376500, 1418200,  1300000,  1170000,        0,  1376500,
		  0x4a9 },
	  /*     604 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1548500, 1595500,  1462500,  1316300,        0,  1548500,
		  0x4aa },
	  /*     605 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1720600, 1772700,  1625000,  1462500,        0,  1720600,
		  0x4ab },
	  /*     606 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1858200, 1914500,  1755000,  1579500,        0,  1858200,
		  0x4ac },
	  /*     607 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2064700, 2127300,  1950000,  1755000,        0,  2064700,
		  0x4ad },
	  /* When number spatial streams > 6
	   * use below rates
	   */
	  /*     608 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  120400, 124100,   113800,   102400,    60200,   120400,
		  0x4c0 },
	  /*     609 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  240900, 248200,   227500,   204800,   120400,   240900,
		  0x4c1 },
	  /*     610 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  361300, 372300,   341300,   307100,   180600,   361300,
		  0x4c2 },
	  /*     611 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  481800, 496400,   455000,   409500,        0,   481800,
		  0x4c3 },
	  /*     612 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  722600, 744500,   682500,   614300,        0,   722600,
		  0x4c4 },
	  /*     613 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  963500, 992700,   910000,   819000,        0,   963500,
		  0x4c5 },
	  /*     614 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1084000, 1116800,  1023800,   921400,        0,  1084000,
		  0x4c6 },
	  /*     615 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1204400, 1240900,  1137500,  1023800,        0,  1204400,
		  0x4c7 },
	  /*     616 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1445300, 1489100,  1365000,  1228500,        0,  1445300,
		  0x4c8 },
	  /*     617 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1605900, 1654500,  1516700,  1365000,        0,  1605900,
		  0x4c9 },
	  /*     618 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1806600, 1861400,  1706300,  1535600,        0,  1806600,
		  0x4ca },
	  /*     619 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2007400, 2068200,  1895800,  1706300,        0,  2007400,
		  0x4cb },
	  /*     620 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2167900, 2233600,  2047500,  1842800,        0,  2167900,
		  0x4cc },
	  /*     621 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2408800, 2481800,  2275000,  2047500,        0,  2408800,
		  0x4cd },

	  /* When number of spatial streams > 7
	   * use below rates
	   */
	  /*     622 MCS-00 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  137600, 141800,   130000,   117000,    68800,   137600,
		  0x4e0 },
	  /*     623 MCS-01 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  275300, 283600,   260000,   234000,   137600,   275300,
		  0x4e1 },
	  /*     624 MCS-02 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  412900, 425500,   390000,   351000,   206500,   412900,
		  0x4e2 },
	  /*     625 MCS-03 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  550600, 567300,   520000,   468000,        0,   550600,
		  0x4e3 },
	  /*     626 MCS-04 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  825900, 850900,   780000,   702000,        0,   825900,
		  0x4e4 },
	  /*     627 MCS-05 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1101200, 1134500,  1040000,   936000,        0,  1101200,
		  0x4e5 },
	  /*     628 MCS-06 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1238800, 1276400,  1170000,  1053000,        0,  1238800,
		  0x4e6 },
	  /*     629 MCS-07 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1376500, 1418200,  1300000,  1170000,        0,  1376500,
		  0x4e7 },
	  /*     630 MCS-08 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1651800, 1701800,  1560000,  1404000,        0,  1651800,
		  0x4e8 },
	  /*     631 MCS-09 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  1835300, 1890900,  1733300,  1560000,        0,  1835300,
		  0x4e9 },
	  /*     632 MCS-10 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2064700, 2127300,  1950000,  1755000,        0,  2064700,
		  0x4ea },
	  /*     633 MCS-11 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2294100, 2363600,  2166700,  1950000,        0,  2294100,
		  0x4eb },
	  /*     634 MCS-12 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2477600, 2552700,  2340000,  2106000,        0,  2477600,
		  0x4ec },
	  /*     635 MCS-13 */ { HE40_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_40,
		  2752900, 2836400,  2600000,  2340000,        0,  2752900,
		  0x4ed },

	  /* 11ax RU996 rates
	   */
	  /*     636 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  36000, 37100,    34000,    30600,    18000,    36000,  0x400
	  },
	  /*     637 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  72100, 74200,    68100,    61300,    36000,    72100,  0x401
	  },
	  /*     638 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  108100, 111400,   102100,    91900,        0,   108100,
		  0x402 },
	  /*     639 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  144100, 148500,   136100,   122500,    72100,   144100,
		  0x403 },
	  /*     640 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  216200, 222700,   204200,   183800,   108100,   216200,
		  0x404 },
	  /*     641 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  288200, 297000,   272200,   245000,        0,   288200,
		  0x405 },
	  /*     642 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  324300, 334100,   306300,   275600,        0,   324300,
		  0x406 },
	  /*     643 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  360300, 371200,   340300,   306300,        0,   360300,
		  0x407 },
	  /*     644 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  432400, 445500,   408300,   367500,        0,   432400,
		  0x408 },
	  /*     645 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  480400, 494900,   453700,   408300,        0,   480400,
		  0x409 },
	  /*     646 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  540400, 556800,   510400,   459400,        0,   540400,
		  0x40a },
	  /*     647 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  600500, 618700,   567100,   510400,        0,   600500,
		  0x40b },
	  /*     648 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  648500, 668200,   612500,   551300,        0,   648500,
		  0x40c },
	  /*     649 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  720600, 742400,   680600,   612500,        0,   720600,
		  0x40d },
	  /* When number spatial streams > 1
	   * use below rates
	   */
	  /*     650 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  72100, 74200,    68100,    61300,    36000,    72100,  0x420
	  },
	  /*     651 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  144100, 148500,   136100,   122500,    72100,   144100,
		  0x421 },
	  /*     652 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  216200, 222700,   204200,   183800,        0,   216200,
		  0x422 },
	  /*     653 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  288200, 297000,   272200,   245000,   144100,   288200,
		  0x423 },
	  /*     654 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  432400, 445500,   408300,   367500,   216200,   432400,
		  0x424 },
	  /*     655 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  576500, 593900,   544400,   490000,        0,   576500,
		  0x425 },
	  /*     656 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  648500, 668200,   612500,   551300,        0,   648500,
		  0x426 },
	  /*     657 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  720600, 742400,   680600,   612500,        0,   720600,
		  0x427 },
	  /*     658 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  864700, 890900,   816700,   735000,        0,   864700,
		  0x428 },
	  /*     659 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  960800, 989900,   907400,   816700,        0,   960800,
		  0x429 },
	  /*     660 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1080900, 1113600,  1020800,   918800,        0,  1080900,
		  0x42a },
	  /*     661 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1201000, 1237400,  1134300,  1020800,        0,  1201000,
		  0x42b },
	  /*     662 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1297100, 1336400,  1225000,  1102500,        0,  1297100,
		  0x42c },
	  /*     663 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1441200, 1484800,  1361100,  1225000,        0,  1441200,
		  0x42d },

	  /* When number of spatial streams > 2
	   * use below rates
	   */
	  /*     664 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  108100, 111400,   102100,    91900,    54000,   108100,
		  0x440 },
	  /*     665 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  216200, 222700,   204200,   183800,   108100,   216200,
		  0x441 },
	  /*     666 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  324300, 334100,   306300,   275600,        0,   324300,
		  0x442 },
	  /*     667 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  432400, 445500,   408300,   367500,        0,   432400,
		  0x443 },
	  /*     668 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  648500, 668200,   612500,   551300,        0,   648500,
		  0x444 },
	  /*     669 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  864700, 890900,   816700,   735000,        0,   864700,
		  0x445 },
	  /*     670 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  972800, 1002300,   918800,   826900,        0,   972800,
		  0x446 },
	  /*     671 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1080900, 1113600,  1020800,   918800,        0,  1080900,
		  0x447 },
	  /*     672 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1297100, 1336400,  1225000,  1102500,        0,  1297100,
		  0x448 },
	  /*     673 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1441200, 1484800,  1361100,  1225000,        0,  1441200,
		  0x449 },
	  /*     674 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1621300, 1670500,  1531300,  1378100,        0,  1621300,
		  0x44a },
	  /*     675 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1801500, 1856100,  1701400,  1531300,        0,  1801500,
		  0x44b },
	  /*     676 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1945600, 2004500,  1837500,  1653800,        0,  1945600,
		  0x44c },
	  /*     677 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2161800, 2227300,  2041700,  1837500,        0,  2161800,
		  0x44d },

	  /* When number of spatial streams > 3
	   * use below rates
	   */
	  /*     678 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  144100, 148500,   136100,   122500,    72100,   144100,
		  0x460 },
	  /*     679 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  288200, 297000,   272200,   245000,   144100,   288200,
		  0x461 },
	  /*     680 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  432400, 445500,   408300,   367500,        0,   432400,
		  0x462 },
	  /*     681 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  576500, 593900,   544400,   490000,   288200,   576500,
		  0x463 },
	  /*     682 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  864700, 890900,   816700,   735000,   432400,   864700,
		  0x464 },
	  /*     683 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1152900, 1187900,  1088900,   980000,        0,  1152900,
		  0x465 },
	  /*     684 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1297100, 1336400,  1225000,  1102500,        0,  1297100,
		  0x466 },
	  /*     685 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1441200, 1484800,  1361100,  1225000,        0,  1441200,
		  0x467 },
	  /*     686 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1729400, 1781800,  1633300,  1470000,        0,  1729400,
		  0x468 },
	  /*     687 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1921600, 1979800,  1814800,  1633300,        0,  1921600,
		  0x469 },
	  /*     688 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2161800, 2227300,  2041700,  1837500,        0,  2161800,
		  0x46a },
	  /*     689 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2401900, 2474700,  2268500,  2041700,        0,  2401900,
		  0x46b },
	  /*     690 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2594100, 2672700,  2450000,  2205000,        0,  2594100,
		  0x46c },
	  /*     691 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2882400, 2969700,  2722200,  2450000,        0,  2882400,
		  0x46d },

	  /* When number spatial streams > 4
	   * use below rates
	   */
	  /*     692 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  180100, 185600,   170100,   153100,    90100,   180100,
		  0x480 },
	  /*     693 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  360300, 371200,   340300,   306300,   180100,   360300,
		  0x481 },
	  /*     694 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  540400, 556800,   510400,   459400,        0,   540400,
		  0x482 },
	  /*     695 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  720600, 742400,   680600,   612500,        0,   720600,
		  0x483 },
	  /*     696 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1080900, 1113600,  1020800,   918800,        0,  1080900,
		  0x484 },
	  /*     697 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1441200, 1484800,  1361100,  1225000,        0,  1441200,
		  0x485 },
	  /*     698 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1621300, 1670500,  1531300,  1378100,        0,  1621300,
		  0x486 },
	  /*     699 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1801500, 1856100,  1701400,  1531300,        0,  1801500,
		  0x487 },
	  /*     700 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2161800, 2227300,  2041700,  1837500,        0,  2161800,
		  0x488 },
	  /*     701 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2402000, 2474700,  2268500,  2041700,        0,  2402000,
		  0x489 },
	  /*     702 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2702200, 2784100,  2552100,  2296900,        0,  2702200,
		  0x48a },
	  /*     703 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3002500, 3093400,  2835600,  2552100,        0,  3002500,
		  0x48b },
	  /*     704 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3242600, 3340900,  3062500,  2756300,        0,  3242600,
		  0x48c },
	  /*     705 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3602900, 3712100,  3402800,  3062500,        0,  3602900,
		  0x48d },

	  /* When number of spatial streams > 5
	   * use below rates
	   */
	  /*     706 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  216200, 222700,   204200,   183800,   108100,   216200,
		  0x4a0 },
	  /*     707 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  432400, 445500,   408300,   367500,   216200,   432400,
		  0x4a1 },
	  /*     708 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  648500, 668200,   612500,   551300,        0,   648500,
		  0x4a2 },
	  /*     709 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  864700, 890900,   816700,   735000,   432400,   864700,
		  0x4a3 },
	  /*     710 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1297100, 1336400,  1225000,  1102500,   648500,  1297100,
		  0x4a4 },
	  /*     711 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1729400, 1781800,  1633300,  1470000,        0,  1729400,
		  0x4a5 },
	  /*     712 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1945600, 2004500,  1837500,  1653800,        0,  1945600,
		  0x4a6 },
	  /*     713 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2161800, 2227300,  2041700,  1837500,        0,  2161800,
		  0x4a7 },
	  /*     714 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2594100, 2672700,  2450000,  2205000,        0,  2594100,
		  0x4a8 },
	  /*     715 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2882400, 2969700,  2722200,  2450000,        0,  2882400,
		  0x4a9 },
	  /*     716 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3242600, 3340900,  3062500,  2756300,        0,  3242600,
		  0x4aa },
	  /*     717 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3602900, 3712100,  3402800,  3062500,        0,  3602900,
		  0x4ab },
	  /*     718 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3891200, 4009100,  3675000,  3307500,        0,  3891200,
		  0x4ac },
	  /*     719 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  4323500, 4454500,  4083300,  3675000,        0,  4323500,
		  0x4ad },

	  /* When number of spatial streams > 6
	   * use below rates
	   */
	  /*     720 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  252200, 259800,   238200,   214400,   129900,   252200,
		  0x4c0 },
	  /*     721 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  504400, 519700,   476400,   428800,   252200,   504400,
		  0x4c1 },
	  /*     722 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  756600, 779500,   714600,   643100,        0,   756600,
		  0x4c2 },
	  /*     723 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1008800, 1039400,   952800,   857500,   504400,  1008800,
		  0x4c3 },
	  /*     724 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1513200, 1559100,  1429200,  1286300,   756600,  1513200,
		  0x4c4 },
	  /*     725 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2017600, 2078800,  1905600,  1715000,        0,  2017600,
		  0x4c5 },
	  /*     726 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2269900, 2338600,  2143800,  1929400,        0,  2269900,
		  0x4c6 },
	  /*     727 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2522100, 2598500,  2381900,  2143800,        0,  2522100,
		  0x4c7 },
	  /*     728 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3026500, 3118200,  2858300,  2572500,        0,  3026500,
		  0x4c8 },
	  /*     729 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3362700, 3464600,  3175900,  2858300,        0,  3362700,
		  0x4c9 },
	  /*     730 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3783100, 3897700,  3572900,  3215600,        0,  3783100,
		  0x4ca },
	  /*     731 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  4203400, 4330800,  3969900,  3572900,        0,  4203400,
		  0x4cb },
	  /*     732 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  4539700, 4677300,  4287500,  3858800,        0,  4539700,
		  0x4cc },
	  /*     733 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  5044100, 5197000,  4763900,  4287500,        0,  5044100,
		  0x4cd },

	  /* When number of spatial streams > 7
	   * use below rates
	   */
	  /*     734 MCS-00 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  288200, 297000,   272200,   245000,   144100,   288200,
		  0x4e0 },
	  /*     735 MCS-01 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  576500, 593900,   544400,   490000,   288200,   576500,
		  0x4e1 },
	  /*     736 MCS-02 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  864700, 890900,   816700,   735000,        0,   864700,
		  0x4e2 },
	  /*     737 MCS-03 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1152900, 1187900,  1088900,   980000,   576500,  1152900,
		  0x4e3 },
	  /*     738 MCS-04 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  1729400, 1781800,  1633300,  1470000,   864700,  1729400,
		  0x4e4 },
	  /*     739 MCS-05 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2305900, 2375800,  2177800,  1960000,        0,  2305900,
		  0x4e5 },
	  /*     740 MCS-06 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2594100, 2672700,  2450000,  2205000,        0,  2594100,
		  0x4e6 },
	  /*     741 MCS-07 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  2882400, 2969700,  2722200,  2450000,        0,  2882400,
		  0x4e7 },
	  /*     742 MCS-08 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3458800, 3563600,  3266700,  2940000,        0,  3458800,
		  0x4e8 },
	  /*     743 MCS-09 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  3843100, 3959600,  3629600,  3266700,        0,  3843100,
		  0x4e9 },
	  /*     744 MCS-10 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  4323500, 4454500,  4083300,  3675000,        0,  4323500,
		  0x4ea },
	  /*     745 MCS-11 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  4803900, 4949500,  4537000,  4083300,        0,  4803900,
		  0x4eb },
	  /*     746 MCS-12 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  5188200, 5345500,  4900000,  4410000,        0,  5188200,
		  0x4ec },
	  /*     747 MCS-13 */ { HE80_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_80,
		  5764700, 5939400,  5444400,  4900000,        0,  5764700,
		  0x4ed },

	  /* 11ax RU996x2 rates
	   */
	  /*     748 MCS-00 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  72100, 74200,    68100,    61300,    36000,    72100,
		  0x400},
	  /*     749 MCS-01 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  144100, 148500,   136100,   122500,    72100,   144100,
		  0x401},
	  /*     750 MCS-02 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  216200, 222700,   204200,   183800,        0,   216200,
		  0x402},
	  /*     751 MCS-03 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  288200, 297000,   272200,   245000,   144100,   288200,
		  0x403},
	  /*     752 MCS-04 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  432400, 445500,   408300,   367500,   216200,   432400,
		  0x404},
	  /*     753 MCS-05 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  576500, 593900,   544400,   490000,        0,   576500,
		  0x405},
	  /*     754 MCS-06 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  648500, 668200,   612500,   551300,        0,   648500,
		  0x406},
	  /*     755 MCS-07 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  720600, 742400,   680600,   612500,        0,   720600,
		  0x407},
	  /*     756 MCS-08 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  864700, 890900,   816700,   735000,        0,   864700,
		  0x408},
	  /*     757 MCS-09 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  960800, 989900,   907400,   816700,        0,   960800,
		  0x409},
	  /*     758 MCS-10 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1080900, 1113600,  1020800,   918800,        0,  1080900,
		  0x40a},
	  /*     759 MCS-11 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1201000, 1237400,  1134300,  1020800,        0,  1201000,
		  0x40b},
	  /*     760 MCS-12 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1297100, 1336400,  1225000,  1102500,        0,  1297100,
		  0x40c},
	  /*     761 MCS-13 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1441200, 1484800,  1361100,  1225000,        0,  1441200,
		  0x40d},
	  /* When maximum spatial streams supported at 160MHZ > 1
	   * use below rates
	   */
	  /*     762 MCS-00 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  144100, 148500,   136100,   122500,    72100,   144100,
		  0x420},
	  /*     763 MCS-01 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  288200, 297000,   272200,   245000,   144100,   288200,
		  0x421},
	  /*     764 MCS-02 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  432400, 445500,   408300,   367500,        0,   432400,
		  0x422},
	  /*     765 MCS-03 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  576500, 593900,   544400,   490000,   288200,   576500,
		  0x423},
	  /*     766 MCS-04 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  864700, 890900,   816700,   735000,   432400,   864700,
		  0x424},
	  /*     767 MCS-05 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1152900, 1187900,  1088900,   980000,        0,  1152900,
		  0x425},
	  /*     768 MCS-06 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1297100, 1336400,  1225000,  1102500,        0,  1297100,
		  0x426},
	  /*     779 MCS-07 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1441200, 1484800,  1361100,  1225000,        0,  1441200,
		  0x427},
	  /*     780 MCS-08 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1729400, 1781800,  1633300,  1470000,        0,  1729400,
		  0x428},
	  /*     781 MCS-09 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1921600, 1979800,  1814800,  1633300,        0,  1921600,
		  0x429},
	  /*     782 MCS-10 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2161800, 2227300,  2041700,  1837500,        0,  2161800,
		  0x42a},
	  /*     783 MCS-11 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2401900, 2474700,  2268500,  2041700,        0,  2401900,
		  0x42b},
	  /*     784 MCS-12 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2594100, 2672700,  2450000,  2205000,        0,  2594100,
		  0x42c},
	  /*     785 MCS-13 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2882400, 2969700,  2722200,  2450000,        0,  2882400,
		  0x42d},

	  /* When maximum spatial streams supported at 160MHZ > 2
	   * use below rates
	   */
	  /*     786 MCS-00 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  216200, 222700,   204200,   183800,   108100,   216200,
		  0x440},
	  /*     787 MCS-01 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  432400, 445500,   408300,   367500,   216200,   432400,
		  0x441},
	  /*     788 MCS-02 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  648500, 668200,   612500,   551300,        0,   648500,
		  0x442},
	  /*     789 MCS-03 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  864700, 890900,   816700,   735000,   432400,   864700,
		  0x443},
	  /*     790 MCS-04 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1297100, 1336400,  1225000,  1102500,   648500,  1297100,
		  0x444},
	  /*     791 MCS-05 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1729400, 1781800,  1633300,  1470000,        0,  1729400,
		  0x445},
	  /*     792 MCS-06 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1945600, 2004500,  1837500,  1653800,        0,  1945600,
		  0x446},
	  /*     793 MCS-07 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2161800, 2227300,  2041700,  1837500,        0,  2161800,
		  0x447},
	  /*     794 MCS-08 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2594100, 2672700,  2450000,  2205000,        0,  2594100,
		  0x448},
	  /*     795 MCS-09 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2882400, 2969700,  2722200,  2450000,        0,  2882400,
		  0x449},
	  /*     796 MCS-10 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  3242600, 3340900,  3062500,  2756300,        0,  3242600,
		  0x44a},
	  /*     797 MCS-11 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  3602900, 3712100,  3402800,  3062500,        0,  3602900,
		  0x44b},
	  /*     798 MCS-12 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  3891200, 4009100,  3675000,  3307500,        0,  3891200,
		  0x44c},
	  /*     799 MCS-13 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  4323500, 4454500,  4083300,  3675000,        0,  4323500,
		  0x44d},

	  /* When maximum spatial streams supported at 160MHZ > 3
	   * use below rates
	   */
	  /*     800 MCS-00 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  288200, 297000,   272200,   245000,   144100,   288200,
		  0x460},
	  /*     801 MCS-01 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  576500, 593900,   544400,   490000,   288200,   576500,
		  0x461},
	  /*     802 MCS-02 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  864700, 890900,   816700,   735000,        0,   864700,
		  0x462},
	  /*     803 MCS-03 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1152900, 1187900,  1088900,   980000,   576500,  1152900,
		  0x463},
	  /*     804 MCS-04 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  1729400, 1781800,  1633300,  1470000,   864700,  1729400,
		  0x464},
	  /*     805 MCS-05 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2305900, 2375800,  2177800,  1960000,        0,  2305900,
		  0x465},
	  /*     806 MCS-06 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2594100, 2672700,  2450000,  2205000,        0,  2594100,
		  0x466},
	  /*     807 MCS-07 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  2882400, 2969700,  2722200,  2450000,        0,  2882400,
		  0x467},
	  /*     808 MCS-08 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  3458800, 3563600,  3266700,  2940000,        0,  3458800,
		  0x468},
	  /*     809 MCS-09 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  3843100, 3959600,  3629600,  3266700,        0,  3843100,
		  0x469},
	  /*     810 MCS-10 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  4323500, 4454500,  4083300,  3675000,        0,  4323500,
		  0x46a},
	  /*     811 MCS-11 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  4803900, 4949500,  4537000,  4083300,        0,  4803900,
		  0x46b},
	  /*     812 MCS-12 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  5188200, 5345500,  4900000,  4410000,        0,  5188200,
		  0x46c},
	  /*     813 MCS-13 */ { HE160_LDPC_ONLY_MASKS,
		  DP_CMN_MOD_IEEE80211_T_HE_160,
		  5764700, 5939400,  5444400,  4900000,        0,  5764700,
		  0x46d}
	},
};

static const uint16_t _rc_idx[DP_CMN_MOD_IEEE80211_T_MAX_PHY] = {
	CCK_RATE_TABLE_INDEX,
	OFDM_RATE_TABLE_INDEX,
	HT_20_RATE_TABLE_INDEX,
	HT_40_RATE_TABLE_INDEX,
	VHT_20_RATE_TABLE_INDEX,
	VHT_40_RATE_TABLE_INDEX,
	VHT_80_RATE_TABLE_INDEX,
	VHT_160_RATE_TABLE_INDEX,
	HE_20_RATE_TABLE_INDEX,
	HE_40_RATE_TABLE_INDEX,
	HE_80_RATE_TABLE_INDEX,
	HE_160_RATE_TABLE_INDEX,
};

/*
 * dp_getmodulation - return rate modulation given code spatial width
 * @pream_type - preamble type
 * @width - bandwidth
 *
 * return - modulation type
 */
enum DP_CMN_MODULATION_TYPE dp_getmodulation(
		uint16_t pream_type,
		uint8_t width)
{
	static const enum DP_CMN_MODULATION_TYPE _vht_bw_mod[] = {
		DP_CMN_MOD_IEEE80211_T_VHT_20,
		DP_CMN_MOD_IEEE80211_T_VHT_40,
		DP_CMN_MOD_IEEE80211_T_VHT_80,
		DP_CMN_MOD_IEEE80211_T_VHT_160
	};

	static const enum DP_CMN_MODULATION_TYPE _he_bw_mod[] = {
		DP_CMN_MOD_IEEE80211_T_HE_20,
		DP_CMN_MOD_IEEE80211_T_HE_40,
		DP_CMN_MOD_IEEE80211_T_HE_80,
		DP_CMN_MOD_IEEE80211_T_HE_160
	};

	enum DP_CMN_MODULATION_TYPE modulation;

	CMN_DP_ASSERT(width < CMN_BW_CNT);

	switch (pream_type) {
	case DP_CMN_RATECODE_PREAM_HT:
		if (width)
			modulation = DP_CMN_MOD_IEEE80211_T_HT_40;
		else
			modulation = DP_CMN_MOD_IEEE80211_T_HT_20;
		break;

	case DP_CMN_RATECODE_PREAM_CCK:
		modulation = DP_CMN_MOD_IEEE80211_T_CCK;
		break;

	case DP_CMN_RATECODE_PREAM_VHT:
		modulation = _vht_bw_mod[width];
		break;

	case DP_CMN_RATECODE_PREAM_HE:
		modulation = _he_bw_mod[width];
		break;

	default:
		modulation = DP_CMN_MOD_IEEE80211_T_OFDM;
		break;
	}

	return modulation;
}

/* dp_getrateindex - calculate ratekbps
 * @mcs - MCS index
 * @nss - NSS 1...8
 * preamble - preamble
 * @bw - Transmission Bandwidth
 * @rix: rate index to be populated
 * @ratecode: ratecode
 *
 * return - rate in kbps
 */
uint32_t
dp_getrateindex(uint32_t gi, uint16_t mcs, uint8_t nss, uint8_t preamble,
		uint8_t bw, uint32_t *rix, uint16_t *ratecode)
{
	uint32_t ratekbps = 0, res = RT_INVALID_INDEX; /* represents failure */
	uint16_t rc;
	enum DP_CMN_MODULATION_TYPE mod;

	/* For error case, where idx exceeds bountry limit */
	*ratecode = 0;
	mod = dp_getmodulation(preamble, bw);
	rc = mcs;

	/* get the base of corresponding rate table  entry */
	res = _rc_idx[mod];

	switch (preamble) {
	case DP_CMN_RATECODE_PREAM_HE:
		res += rc + nss * NUM_HE_MCS;
		break;

	case DP_CMN_RATECODE_PREAM_VHT:
		res += rc + nss * NUM_VHT_MCS;
		break;

	case DP_CMN_RATECODE_PREAM_HT:
		res += rc + nss * NUM_HT_MCS;
		break;

	case DP_CMN_RATECODE_PREAM_CCK:
		rc  &= ~HW_RATECODE_CCK_SHORT_PREAM_MASK;
		res += rc;
		break;

	case DP_CMN_RATECODE_PREAM_OFDM:
		res += rc;
		break;

	default:
		break;
	}
	if (res >= DP_RATE_TABLE_SIZE)
		goto done;

	if (!gi) {
		ratekbps = dp_11abgnratetable.info[res].userratekbps;
	} else {
		switch (gi) {
		case CDP_SGI_0_4_US:
			ratekbps = dp_11abgnratetable.info[res].ratekbpssgi;
			break;
		case CDP_SGI_1_6_US:
			ratekbps = dp_11abgnratetable.info[res].ratekbpsdgi;
			break;
		case CDP_SGI_3_2_US:
			ratekbps = dp_11abgnratetable.info[res].ratekbpsqgi;
			break;
		}
	}
	*ratecode = dp_11abgnratetable.info[res].ratecode;
done:
	*rix = res;

	return ratekbps;
}

qdf_export_symbol(dp_getrateindex);

/* dp_rate_idx_to_kbps - get rate kbps from index
 * @rate_idx - rate index
 * @gintval - guard interval
 *
 * return - rate index in kbps with help of ratetable
 */
int dp_rate_idx_to_kbps(uint8_t rate_idx, uint8_t gintval)
{
	if (rate_idx >= DP_RATE_TABLE_SIZE)
		return 0;

	if (!gintval)
		return RT_GET_RAW_KBPS(&dp_11abgnratetable, rate_idx);
	else
		return RT_GET_SGI_KBPS(&dp_11abgnratetable, rate_idx);
	return 0;
}

qdf_export_symbol(dp_rate_idx_to_kbps);

/* dp_get_start_index - get start index as per bw, mode and nss
 * @ch_width - channel bandwidth
 * @mode - operating mode
 * @nss - no. of spatial streams
 *
 * return - start index
 */
static int dp_get_start_index(int ch_width, int mode, int nss)
{
	if (mode == HW_RATECODE_PREAM_HT) {
		if (nss >= NUM_HT_SPATIAL_STREAM)
			nss = NUM_HT_SPATIAL_STREAM;

		if (ch_width == CMN_BW_20MHZ)
			return HT_20_RATE_TABLE_INDEX + (nss - 1) * NUM_HT_MCS;
		else if (ch_width == CMN_BW_40MHZ)
			return HT_40_RATE_TABLE_INDEX + (nss - 1) * NUM_HT_MCS;
	} else if (mode == HW_RATECODE_PREAM_VHT) {
		if (nss >= NUM_SPATIAL_STREAMS)
			nss = NUM_SPATIAL_STREAMS;

		if (ch_width == CMN_BW_20MHZ) {
			return VHT_20_RATE_TABLE_INDEX + (nss - 1) * NUM_VHT_MCS;
		} else if (ch_width == CMN_BW_40MHZ) {
			return VHT_40_RATE_TABLE_INDEX + (nss - 1) * NUM_VHT_MCS;
		} else if (ch_width == CMN_BW_80MHZ) {
			return VHT_80_RATE_TABLE_INDEX + (nss - 1) * NUM_VHT_MCS;
		} else if ((ch_width == CMN_BW_160MHZ) ||
			   (ch_width == CMN_BW_80_80MHZ)) {
			if (nss >= MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ)
				nss = MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ;

			return VHT_160_RATE_TABLE_INDEX + (nss - 1) * NUM_VHT_MCS;
		}
	} else if (mode == HW_RATECODE_PREAM_HE) {
		if (nss >= NUM_SPATIAL_STREAMS)
			nss = NUM_SPATIAL_STREAMS;

		if (ch_width == CMN_BW_20MHZ) {
			return HE_20_RATE_TABLE_INDEX + (nss - 1) * NUM_HE_MCS;
		} else if (ch_width == CMN_BW_40MHZ) {
			return HE_40_RATE_TABLE_INDEX + (nss - 1) * NUM_HE_MCS;
		} else if (ch_width == CMN_BW_80MHZ) {
			return HE_80_RATE_TABLE_INDEX + (nss - 1) * NUM_HE_MCS;
		} else if ((ch_width == CMN_BW_160MHZ) ||
			 (ch_width == CMN_BW_80_80MHZ)) {
			if (nss >= MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ)
				nss = MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ;

			return HE_160_RATE_TABLE_INDEX + (nss - 1) * NUM_HE_MCS;
		}
	}

	return -1;
}

/* dp_get_end_index - get end index as per bw, mode and nss
 * @ch_width - channel bandwidth
 * @mode - operating mode
 * @nss - no. of spatial streams
 *
 * return - end index
 */
static int dp_get_end_index(int ch_width, int mode, int nss)
{
	if (mode == HW_RATECODE_PREAM_HT) {
		if (nss >= NUM_HT_SPATIAL_STREAM)
			nss = NUM_HT_SPATIAL_STREAM;

		if (ch_width == CMN_BW_20MHZ)
			return HT_20_RATE_TABLE_INDEX + nss * NUM_HT_MCS - 1;
		else if (ch_width == CMN_BW_40MHZ)
			return HT_40_RATE_TABLE_INDEX + nss * NUM_HT_MCS - 1;
	} else if (mode == HW_RATECODE_PREAM_VHT) {
		if (nss >= NUM_SPATIAL_STREAMS)
			nss = NUM_SPATIAL_STREAMS;

		if (ch_width == CMN_BW_20MHZ) {
			return VHT_20_RATE_TABLE_INDEX + nss * NUM_VHT_MCS - 1;
		} else if (ch_width == CMN_BW_40MHZ) {
			return VHT_40_RATE_TABLE_INDEX + nss * NUM_VHT_MCS - 1;
		} else if (ch_width == CMN_BW_80MHZ) {
			return VHT_80_RATE_TABLE_INDEX + nss * NUM_VHT_MCS - 1;
		} else if ((ch_width == CMN_BW_160MHZ) ||
			   (ch_width == CMN_BW_80_80MHZ)) {
			if (nss >= MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ)
				nss = MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ;

			return VHT_160_RATE_TABLE_INDEX + nss * NUM_VHT_MCS - 1;
		}
	} else if (mode == HW_RATECODE_PREAM_HE) {
		if (nss >= NUM_SPATIAL_STREAMS)
			nss = NUM_SPATIAL_STREAMS;

		if (ch_width == CMN_BW_20MHZ) {
			return HE_20_RATE_TABLE_INDEX + nss * NUM_HE_MCS - 1;
		} else if (ch_width == CMN_BW_40MHZ) {
			return HE_40_RATE_TABLE_INDEX + nss * NUM_HE_MCS - 1;
		} else if (ch_width == CMN_BW_80MHZ) {
			return HE_80_RATE_TABLE_INDEX + nss * NUM_HE_MCS - 1;
		} else if ((ch_width == CMN_BW_160MHZ) ||
			   (ch_width == CMN_BW_80_80MHZ)) {
			if (nss >= MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ)
				nss = MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ;

			return HE_160_RATE_TABLE_INDEX + nss * NUM_HE_MCS - 1;
		}
	}

	return -1;
}

/* __dp_get_supported_rates - get supported rates as per start and end index
 * @shortgi - gi setting
 * @start_index - starting index
 * @end_index - ending index
 * @rates - array to copy the rates into
 *
 * return - no. of rate entries copied
 */
static int __dp_get_supported_rates(int shortgi, int start_index,
				    int end_index, int **rates)
{
	int i, j = 1;
	int *ratelist = *rates;

	/* Check if the index calculation is out of array bounds */
	if (start_index < 0 || start_index >= DP_RATE_TABLE_SIZE  ||
	    end_index < 0 || end_index >= DP_RATE_TABLE_SIZE)
		return 0;

	if (!shortgi) {
		for (i = start_index; i <= end_index; i++) {
			if (dp_11abgnratetable.info[i].validmodemask) {
				ratelist[j] = dp_11abgnratetable.info[i].
								ratekbps;
				j++;
			}
		}
	} else {
		switch (shortgi) {
		case CDP_SGI_0_4_US:
			for (i = start_index; i <= end_index; i++) {
				if (dp_11abgnratetable.info[i].validmodemask) {
					ratelist[j] = dp_11abgnratetable.
							info[i].ratekbpssgi;
					j++;
				}
			}
			break;

		case CDP_SGI_1_6_US:
			for (i = start_index; i <= end_index; i++) {
				if (dp_11abgnratetable.info[i].validmodemask) {
					ratelist[j] = dp_11abgnratetable.
							info[i].ratekbpsdgi;
					j++;
				}
			}
			break;

		case CDP_SGI_3_2_US:
			for (i = start_index; i <= end_index; i++) {
				if (dp_11abgnratetable.info[i].validmodemask) {
					ratelist[j] = dp_11abgnratetable.
							info[i].ratekbpsqgi;
					j++;
				}
			}
			break;
		}
	}

	ratelist[0] = j;
	return j;
}

#if ALL_POSSIBLE_RATES_SUPPORTED
/* dp_get_supported_rates -get all supported rates as per mode and gi setting
 * @mode - operating mode
 * @shortgi - gi setting
 * @rates - array to copy the rate entries into
 *
 * return - no. of rate entries copied
 */
int dp_get_supported_rates(int mode, int shortgi, int **rates)
{
	int start_index = -1, end_index = -1;

	switch (mode) {
	/* 11b CCK Rates */
	case CMN_IEEE80211_MODE_B:
		start_index = CCK_RATE_TABLE_INDEX;
		end_index = CCK_RATE_TABLE_END_INDEX;
		break;

	/* 11a OFDM Rates */
	case CMN_IEEE80211_MODE_A:
		start_index = OFDM_RATE_TABLE_INDEX;
		end_index = OFDMA_RATE_TABLE_END_INDEX;
		break;

	/* 11g CCK/OFDM Rates */
	case CMN_IEEE80211_MODE_G:
		start_index = CCK_RATE_TABLE_INDEX;
		end_index = OFDMA_RATE_TABLE_END_INDEX;
		break;

	/* HT rates only */
	case CMN_IEEE80211_MODE_NA:
	case CMN_IEEE80211_MODE_NG:
		start_index = dp_get_start_index(CMN_BW_20MHZ,
						 HW_RATECODE_PREAM_HT, 1);
		end_index = dp_get_end_index(CMN_BW_40MHZ,
					     HW_RATECODE_PREAM_HT,
					     NUM_HT_SPATIAL_STREAM);
		break;

	/* VHT rates only */
	case CMN_IEEE80211_MODE_AC:
		start_index = dp_get_start_index(CMN_BW_20MHZ,
						 HW_RATECODE_PREAM_VHT, 1);
		end_index = dp_get_end_index(CMN_BW_160MHZ,
					     HW_RATECODE_PREAM_VHT,
					     MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ);
		break;

	/* HE rates only */
	case CMN_IEEE80211_MODE_AXA:
	case CMN_IEEE80211_MODE_AXG:
		start_index = dp_get_start_index(CMN_BW_20MHZ,
						 HW_RATECODE_PREAM_HE, 1);
		end_index = dp_get_end_index(CMN_BW_160MHZ,
					     HW_RATECODE_PREAM_HE,
					     MAX_SPATIAL_STREAMS_SUPPORTED_AT_160MHZ);
		break;
	}

	return __dp_get_supported_rates(shortgi, start_index, end_index, rates);
}
#else
/* dp_get_supported_rates - get all supported rates as per mode, bw, gi and nss
 * @mode - operating mode
 * @shortgi - gi setting
 * @nss - no. of spatial streams
 * @ch_width - channel bandwidth
 * @rates - array to copy the rates into
 *
 * return - no. of rate entries copied
 */
int dp_get_supported_rates(int mode, int shortgi, int nss,
			   int ch_width, int **rates)
{
	int start_index = -1, end_index = -1;

	switch (mode) {
	/* 11b CCK Rates */
	case CMN_IEEE80211_MODE_B:
		start_index = CCK_RATE_TABLE_INDEX;
		end_index = CCK_RATE_TABLE_END_INDEX;
		break;

	/* 11a OFDM Rates */
	case CMN_IEEE80211_MODE_A:
		start_index = OFDM_RATE_TABLE_INDEX;
		end_index = OFDMA_RATE_TABLE_END_INDEX;
		break;

	/* 11g CCK/OFDM Rates */
	case CMN_IEEE80211_MODE_G:
		start_index = CCK_RATE_TABLE_INDEX;
		end_index = OFDMA_RATE_TABLE_END_INDEX;
		break;

	/* HT rates only */
	case CMN_IEEE80211_MODE_NA:
	case CMN_IEEE80211_MODE_NG:
		start_index = dp_get_start_index(ch_width,
						 HW_RATECODE_PREAM_HT, nss);
		end_index = dp_get_end_index(ch_width,
					     HW_RATECODE_PREAM_HT, nss);
		break;

	/* VHT rates only */
	case CMN_IEEE80211_MODE_AC:
		start_index = dp_get_start_index(ch_width,
						 HW_RATECODE_PREAM_VHT, nss);
		end_index = dp_get_end_index(ch_width,
					     HW_RATECODE_PREAM_VHT, nss);
		break;

	/* HE rates only */
	case CMN_IEEE80211_MODE_AXA:
	case CMN_IEEE80211_MODE_AXG:
		start_index = dp_get_start_index(ch_width,
						 HW_RATECODE_PREAM_HE, nss);
		end_index = dp_get_end_index(ch_width,
					     HW_RATECODE_PREAM_HE, nss);
		break;
	}

	return __dp_get_supported_rates(shortgi, start_index, end_index, rates);
}
#endif

qdf_export_symbol(dp_get_supported_rates);
