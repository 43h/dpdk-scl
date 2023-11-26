/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 * Copyright (c) 2019 Intel Corporation
 */

#ifndef TEST_CRYPTODEV_MOD_TEST_VECTORS_H_
#define TEST_CRYPTODEV_MOD_TEST_VECTORS_H_

#define DATA_SIZE 512

struct modex_test_data {
	enum rte_crypto_asym_xform_type xform_type;
	const char *description;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} base;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} exponent;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} modulus;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} reminder;
	uint16_t result_len;
};
struct modinv_test_data {
	enum rte_crypto_asym_xform_type xform_type;
	const char *description;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} base;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} modulus;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} inverse;
	uint16_t result_len;
};

/* ModExp #1 */
static const struct
modex_test_data modex_test_case_m128_b20_e3 = {
	.description =
		"Modular Exponentiation (mod=128, base=20, exp=3, res=128)",
	.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX,
	.base = {
		.data = {
			0xF8, 0xBA, 0x1A, 0x55, 0xD0, 0x2F, 0x85,
			0xAE, 0x96, 0x7B, 0xB6, 0x2F, 0xB6, 0xCD,
			0xA8, 0xEB, 0x7E, 0x78, 0xA0, 0x50
		},
		.len = 20
	},
	.exponent = {
		.data = {
			0x01, 0x00, 0x01
		},
		.len = 3
	},
	.reminder = {
		.data = {
			0x2C, 0x60, 0x75, 0x45, 0x98, 0x9D, 0xE0, 0x72,
			0xA0, 0x9D, 0x3A, 0x9E, 0x03, 0x38, 0x73, 0x3C,
			0x31, 0x83, 0x04, 0xFE, 0x75, 0x43, 0xE6, 0x17,
			0x5C, 0x01, 0x29, 0x51, 0x69, 0x33, 0x62, 0x2D,
			0x78, 0xBE, 0xAE, 0xC4, 0xBC, 0xDE, 0x7E, 0x2C,
			0x77, 0x84, 0xF2, 0xC5, 0x14, 0xB5, 0x2F, 0xF7,
			0xC5, 0x94, 0xEF, 0x86, 0x75, 0x75, 0xB5, 0x11,
			0xE5, 0x0E, 0x0A, 0x29, 0x76, 0xE2, 0xEA, 0x32,
			0x0E, 0x43, 0x77, 0x7E, 0x2C, 0x27, 0xAC, 0x3B,
			0x86, 0xA5, 0xDB, 0xC9, 0x48, 0x40, 0xE8, 0x99,
			0x9A, 0x0A, 0x3D, 0xD6, 0x74, 0xFA, 0x2E, 0x2E,
			0x5B, 0xAF, 0x8C, 0x99, 0x44, 0x2A, 0x67, 0x38,
			0x27, 0x41, 0x59, 0x9D, 0xB8, 0x51, 0xC9, 0xF7,
			0x43, 0x61, 0x31, 0x6E, 0xF1, 0x25, 0x38, 0x7F,
			0xAE, 0xC6, 0xD0, 0xBB, 0x29, 0x76, 0x3F, 0x46,
			0x2E, 0x1B, 0xE4, 0x67, 0x71, 0xE3, 0x87, 0x5A
		},
		.len = 128
	},
	.modulus = {
		.data = {
			0xb3, 0xa1, 0xaf, 0xb7, 0x13, 0x08, 0x00, 0x0a,
			0x35, 0xdc, 0x2b, 0x20, 0x8d, 0xa1, 0xb5, 0xce,
			0x47, 0x8a, 0xc3, 0x80, 0xf4, 0x7d, 0x4a, 0xa2,
			0x62, 0xfd, 0x61, 0x7f, 0xb5, 0xa8, 0xde, 0x0a,
			0x17, 0x97, 0xa0, 0xbf, 0xdf, 0x56, 0x5a, 0x3d,
			0x51, 0x56, 0x4f, 0x70, 0x70, 0x3f, 0x63, 0x6a,
			0x44, 0x5b, 0xad, 0x84, 0x0d, 0x3f, 0x27, 0x6e,
			0x3b, 0x34, 0x91, 0x60, 0x14, 0xb9, 0xaa, 0x72,
			0xfd, 0xa3, 0x64, 0xd2, 0x03, 0xa7, 0x53, 0x87,
			0x9e, 0x88, 0x0b, 0xc1, 0x14, 0x93, 0x1a, 0x62,
			0xff, 0xb1, 0x5d, 0x74, 0xcd, 0x59, 0x63, 0x18,
			0x11, 0x3d, 0x4f, 0xba, 0x75, 0xd4, 0x33, 0x4e,
			0x23, 0x6b, 0x7b, 0x57, 0x44, 0xe1, 0xd3, 0x03,
			0x13, 0xa6, 0xf0, 0x8b, 0x60, 0xb0, 0x9e, 0xee,
			0x75, 0x08, 0x9d, 0x71, 0x63, 0x13, 0xcb, 0xa6,
			0x81, 0x92, 0x14, 0x03, 0x22, 0x2d, 0xde, 0x55
		},
		.len = 128
	},
	.result_len = 128
};

/* ModInv #1 */
static const struct
modinv_test_data modinv_test_case = {
	.description = "Modular Inverse (mod=128, base=20, exp=3, inv=128)",
	.xform_type = RTE_CRYPTO_ASYM_XFORM_MODINV,
	.base = {
		.data = {
			0xF8, 0xBA, 0x1A, 0x55, 0xD0, 0x2F, 0x85,
			0xAE, 0x96, 0x7B, 0xB6, 0x2F, 0xB6, 0xCD,
			0xA8, 0xEB, 0x7E, 0x78, 0xA0, 0x50
		},
		.len = 20
	},
	.inverse = {
		.data = {
			0x52, 0xb1, 0xa3, 0x8c, 0xc5, 0x8a, 0xb9, 0x1f,
			0xb6, 0x82, 0xf5, 0x6a, 0x9a, 0xde, 0x8d, 0x2e,
			0x62, 0x4b, 0xac, 0x49, 0x21, 0x1d, 0x30, 0x4d,
			0x32, 0xac, 0x1f, 0x40, 0x6d, 0x52, 0xc7, 0x9b,
			0x6c, 0x0a, 0x82, 0x3a, 0x2c, 0xaf, 0x6b, 0x6d,
			0x17, 0xbe, 0x43, 0xed, 0x97, 0x78, 0xeb, 0x4c,
			0x92, 0x6f, 0xcf, 0xed, 0xb1, 0x09, 0xcb, 0x27,
			0xc2, 0xde, 0x62, 0xfd, 0x21, 0xe6, 0xbd, 0x4f,
			0xfe, 0x7a, 0x1b, 0x50, 0xfe, 0x10, 0x4a, 0xb0,
			0xb7, 0xcf, 0xdb, 0x7d, 0xca, 0xc2, 0xf0, 0x1c,
			0x39, 0x48, 0x6a, 0xb5, 0x4d, 0x8c, 0xfe, 0x63,
			0x91, 0x9c, 0x21, 0xc3, 0x0e, 0x76, 0xad, 0x44,
			0x8d, 0x54, 0x33, 0x99, 0xe1, 0x80, 0x19, 0xba,
			0xb5, 0xac, 0x7d, 0x9c, 0xce, 0x91, 0x2a, 0xd9,
			0x2c, 0xe1, 0x16, 0xd6, 0xd7, 0xcf, 0x9d, 0x05,
			0x9a, 0x66, 0x9a, 0x3a, 0xc1, 0xb8, 0x4b, 0xc3
		},
		.len = 128
	},
	.modulus = {
		.data = {
			0xb3, 0xa1, 0xaf, 0xb7, 0x13, 0x08, 0x00, 0x0a,
			0x35, 0xdc, 0x2b, 0x20, 0x8d, 0xa1, 0xb5, 0xce,
			0x47, 0x8a, 0xc3, 0x80, 0xf4, 0x7d, 0x4a, 0xa2,
			0x62, 0xfd, 0x61, 0x7f, 0xb5, 0xa8, 0xde, 0x0a,
			0x17, 0x97, 0xa0, 0xbf, 0xdf, 0x56, 0x5a, 0x3d,
			0x51, 0x56, 0x4f, 0x70, 0x70, 0x3f, 0x63, 0x6a,
			0x44, 0x5b, 0xad, 0x84, 0x0d, 0x3f, 0x27, 0x6e,
			0x3b, 0x34, 0x91, 0x60, 0x14, 0xb9, 0xaa, 0x72,
			0xfd, 0xa3, 0x64, 0xd2, 0x03, 0xa7, 0x53, 0x87,
			0x9e, 0x88, 0x0b, 0xc1, 0x14, 0x93, 0x1a, 0x62,
			0xff, 0xb1, 0x5d, 0x74, 0xcd, 0x59, 0x63, 0x18,
			0x11, 0x3d, 0x4f, 0xba, 0x75, 0xd4, 0x33, 0x4e,
			0x23, 0x6b, 0x7b, 0x57, 0x44, 0xe1, 0xd3, 0x03,
			0x13, 0xa6, 0xf0, 0x8b, 0x60, 0xb0, 0x9e, 0xee,
			0x75, 0x08, 0x9d, 0x71, 0x63, 0x13, 0xcb, 0xa6,
			0x81, 0x92, 0x14, 0x03, 0x22, 0x2d, 0xde, 0x55
		},
		.len = 128
	},
	.result_len = 128
};

/* modular operation test data */
uint8_t base[] = {
	0xF8, 0xBA, 0x1A, 0x55, 0xD0, 0x2F, 0x85,
	0xAE, 0x96, 0x7B, 0xB6, 0x2F, 0xB6, 0xCD,
	0xA8, 0xEB, 0x7E, 0x78, 0xA0, 0x50
};

/* MODEX data. 8< */
uint8_t mod_p[] = {
	0x00, 0xb3, 0xa1, 0xaf, 0xb7, 0x13, 0x08, 0x00,
	0x0a, 0x35, 0xdc, 0x2b, 0x20, 0x8d, 0xa1, 0xb5,
	0xce, 0x47, 0x8a, 0xc3, 0x80, 0xf4, 0x7d, 0x4a,
	0xa2, 0x62, 0xfd, 0x61, 0x7f, 0xb5, 0xa8, 0xde,
	0x0a, 0x17, 0x97, 0xa0, 0xbf, 0xdf, 0x56, 0x5a,
	0x3d, 0x51, 0x56, 0x4f, 0x70, 0x70, 0x3f, 0x63,
	0x6a, 0x44, 0x5b, 0xad, 0x84, 0x0d, 0x3f, 0x27,
	0x6e, 0x3b, 0x34, 0x91, 0x60, 0x14, 0xb9, 0xaa,
	0x72, 0xfd, 0xa3, 0x64, 0xd2, 0x03, 0xa7, 0x53,
	0x87, 0x9e, 0x88, 0x0b, 0xc1, 0x14, 0x93, 0x1a,
	0x62, 0xff, 0xb1, 0x5d, 0x74, 0xcd, 0x59, 0x63,
	0x18, 0x11, 0x3d, 0x4f, 0xba, 0x75, 0xd4, 0x33,
	0x4e, 0x23, 0x6b, 0x7b, 0x57, 0x44, 0xe1, 0xd3,
	0x03, 0x13, 0xa6, 0xf0, 0x8b, 0x60, 0xb0, 0x9e,
	0xee, 0x75, 0x08, 0x9d, 0x71, 0x63, 0x13, 0xcb,
	0xa6, 0x81, 0x92, 0x14, 0x03, 0x22, 0x2d, 0xde,
	0x55
};

uint8_t mod_e[] = {0x01, 0x00, 0x01};
/* >8 End of MODEX data. */

/* Precomputed modular exponentiation for verification */
uint8_t mod_exp[] = {
	0x2C, 0x60, 0x75, 0x45, 0x98, 0x9D, 0xE0, 0x72,
	0xA0, 0x9D, 0x3A, 0x9E, 0x03, 0x38, 0x73, 0x3C,
	0x31, 0x83, 0x04, 0xFE, 0x75, 0x43, 0xE6, 0x17,
	0x5C, 0x01, 0x29, 0x51, 0x69, 0x33, 0x62, 0x2D,
	0x78, 0xBE, 0xAE, 0xC4, 0xBC, 0xDE, 0x7E, 0x2C,
	0x77, 0x84, 0xF2, 0xC5, 0x14, 0xB5, 0x2F, 0xF7,
	0xC5, 0x94, 0xEF, 0x86, 0x75, 0x75, 0xB5, 0x11,
	0xE5, 0x0E, 0x0A, 0x29, 0x76, 0xE2, 0xEA, 0x32,
	0x0E, 0x43, 0x77, 0x7E, 0x2C, 0x27, 0xAC, 0x3B,
	0x86, 0xA5, 0xDB, 0xC9, 0x48, 0x40, 0xE8, 0x99,
	0x9A, 0x0A, 0x3D, 0xD6, 0x74, 0xFA, 0x2E, 0x2E,
	0x5B, 0xAF, 0x8C, 0x99, 0x44, 0x2A, 0x67, 0x38,
	0x27, 0x41, 0x59, 0x9D, 0xB8, 0x51, 0xC9, 0xF7,
	0x43, 0x61, 0x31, 0x6E, 0xF1, 0x25, 0x38, 0x7F,
	0xAE, 0xC6, 0xD0, 0xBB, 0x29, 0x76, 0x3F, 0x46,
	0x2E, 0x1B, 0xE4, 0x67, 0x71, 0xE3, 0x87, 0x5A
};

/* Precomputed modular inverse for verification */
uint8_t mod_inv[] = {
	0x52, 0xb1, 0xa3, 0x8c, 0xc5, 0x8a, 0xb9, 0x1f,
	0xb6, 0x82, 0xf5, 0x6a, 0x9a, 0xde, 0x8d, 0x2e,
	0x62, 0x4b, 0xac, 0x49, 0x21, 0x1d, 0x30, 0x4d,
	0x32, 0xac, 0x1f, 0x40, 0x6d, 0x52, 0xc7, 0x9b,
	0x6c, 0x0a, 0x82, 0x3a, 0x2c, 0xaf, 0x6b, 0x6d,
	0x17, 0xbe, 0x43, 0xed, 0x97, 0x78, 0xeb, 0x4c,
	0x92, 0x6f, 0xcf, 0xed, 0xb1, 0x09, 0xcb, 0x27,
	0xc2, 0xde, 0x62, 0xfd, 0x21, 0xe6, 0xbd, 0x4f,
	0xfe, 0x7a, 0x1b, 0x50, 0xfe, 0x10, 0x4a, 0xb0,
	0xb7, 0xcf, 0xdb, 0x7d, 0xca, 0xc2, 0xf0, 0x1c,
	0x39, 0x48, 0x6a, 0xb5, 0x4d, 0x8c, 0xfe, 0x63,
	0x91, 0x9c, 0x21, 0xc3, 0x0e, 0x76, 0xad, 0x44,
	0x8d, 0x54, 0x33, 0x99, 0xe1, 0x80, 0x19, 0xba,
	0xb5, 0xac, 0x7d, 0x9c, 0xce, 0x91, 0x2a, 0xd9,
	0x2c, 0xe1, 0x16, 0xd6, 0xd7, 0xcf, 0x9d, 0x05,
	0x9a, 0x66, 0x9a, 0x3a, 0xc1, 0xb8, 0x4b, 0xc3
};

/* MODEX vector. 8< */
struct rte_crypto_asym_xform modex_xform = {
	.next = NULL,
	.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX,
	.modex = {
		.modulus = {
			.data = mod_p,
			.length = sizeof(mod_p)
		},
		.exponent = {
			.data = mod_e,
			.length = sizeof(mod_e)
		}
	}
};
/* >8 End of MODEX vector. */

struct rte_crypto_asym_xform modinv_xform = {
	.next = NULL,
	.xform_type = RTE_CRYPTO_ASYM_XFORM_MODINV,
	.modinv = {
		.modulus = {
			.data = mod_p,
			.length = sizeof(mod_p)
		}
	}
};

#endif /* TEST_CRYPTODEV_MOD_TEST_VECTORS_H__ */