/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <sid_pal_mfg_store_ifc.h>
#include <sid_error.h>

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/byteorder.h>

#define MFG_VERSION_1_VAL   0x01000000

/* Flash block size in bytes */
#define MFG_STORAGE_SIZE  (DT_PROP(DT_CHOSEN(zephyr_flash), erase_block_size))
/**
 * Last block in the flash storage will be used for manufacturing storage.
 * MFG_START_OFFSET = 0x0FF000
 * MFG_END_OFFSET   = 0x100000
 */
#define MFG_END_OFFSET (FIXED_PARTITION_OFFSET(sidewalk_storage) + FIXED_PARTITION_SIZE(sidewalk_storage))
#define MFG_START_OFFSET (MFG_END_OFFSET - MFG_STORAGE_SIZE)

static uint8_t test_data_buffer[512];

/******************************************************************
* NOTE: DO NOT CHANGE THE ORDER OF THE TESTS!
* ****************************************************************/
static void hton_buff(uint8_t *buffer, size_t buff_len)
{
	uint32_t val_l = 0;
	size_t i = 0;
	uint8_t mod_len = buff_len % sizeof(uint32_t);

	if (sizeof(uint32_t) <= buff_len) {
		for (i = 0; i < (buff_len - mod_len); i += sizeof(uint32_t)) {
			memcpy(&val_l, &buffer[i], sizeof(val_l));
			val_l = sys_cpu_to_be32(val_l);
			memcpy(&buffer[i], &val_l, sizeof(val_l));
		}
	}
	// if 1 == mod_len then last byte shall be copied directly do destination buffer.
	if (2 == mod_len) {
		uint16_t val_s = 0;
		memcpy(&val_s, &buffer[i], sizeof(val_s));
		val_s = sys_cpu_to_be16(val_s);
		memcpy(&buffer[i], &val_s, sizeof(val_s));
	} else if (3 == mod_len) {
		val_l = 0;
		memcpy(&val_l, &buffer[i], 3 * sizeof(uint8_t));
		val_l = sys_cpu_to_be24(val_l);
		memcpy(&buffer[i], &val_l, 3 * sizeof(uint8_t));
	}
}

static void mfg_set_version(uint32_t version)
{
	uint8_t write_buff[SID_PAL_MFG_STORE_MAX_FLASH_WRITE_LEN];

	memcpy(write_buff, &version, SID_PAL_MFG_STORE_VERSION_SIZE);
	TEST_ASSERT_EQUAL(0,
			  sid_pal_mfg_store_write(SID_PAL_MFG_STORE_VERSION, write_buff,
						  SID_PAL_MFG_STORE_VERSION_SIZE));
	version = sys_cpu_to_be32(version);
	TEST_ASSERT_EQUAL(version, sid_pal_mfg_store_get_version());
}

static void mfg_clr_memory(void)
{
	TEST_ASSERT_EQUAL(0, sid_pal_mfg_store_erase());
	TEST_ASSERT_TRUE(sid_pal_mfg_store_is_empty());
}

void test_sid_pal_mfg_storage_no_init(void)
{
	uint8_t read_buffer[SID_PAL_MFG_STORE_VERSION_SIZE] = { 0 };

	TEST_ASSERT_EQUAL(SID_ERROR_UNINITIALIZED,
			  sid_pal_mfg_store_write(SID_PAL_MFG_STORE_VERSION, read_buffer,
						  SID_PAL_MFG_STORE_VERSION_SIZE));
	TEST_ASSERT_EQUAL(SID_ERROR_UNINITIALIZED, sid_pal_mfg_store_erase());
	TEST_ASSERT_FALSE(sid_pal_mfg_store_is_empty());
	memset(read_buffer, 0xAA, SID_PAL_MFG_STORE_VERSION_SIZE);
	memset(test_data_buffer, 0xAA, SID_PAL_MFG_STORE_VERSION_SIZE);
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_VERSION, read_buffer, SID_PAL_MFG_STORE_VERSION_SIZE);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(read_buffer, test_data_buffer, SID_PAL_MFG_STORE_VERSION_SIZE);
}

void test_sid_pal_mfg_storage_init(void)
{
	static const sid_pal_mfg_store_region_t mfg_store_region = {
		.addr_start = MFG_START_OFFSET,
		.addr_end = MFG_END_OFFSET,
	};

	sid_pal_mfg_store_init(mfg_store_region);
	TEST_ASSERT_EQUAL(0, sid_pal_mfg_store_erase());
}

void test_sid_pal_mfg_storage_write(void)
{
	uint8_t write_buff[SID_PAL_MFG_STORE_MAX_FLASH_WRITE_LEN] = { 0 };

	TEST_ASSERT_EQUAL(SID_ERROR_INVALID_ARGS, sid_pal_mfg_store_write(SID_PAL_MFG_STORE_VERSION, write_buff, 0));
	TEST_ASSERT_EQUAL(SID_ERROR_OUT_OF_RESOURCES, sid_pal_mfg_store_write(SID_PAL_MFG_STORE_VERSION, write_buff,
									      128));
	TEST_ASSERT_EQUAL(SID_ERROR_INCOMPATIBLE_PARAMS,
			  sid_pal_mfg_store_write(SID_PAL_MFG_STORE_VERSION, write_buff, 3));
	TEST_ASSERT_EQUAL(SID_ERROR_NOT_FOUND,
			  sid_pal_mfg_store_write(999, write_buff, SID_PAL_MFG_STORE_VERSION_SIZE));
	TEST_ASSERT_EQUAL(SID_ERROR_NULL_POINTER,
			  sid_pal_mfg_store_write(SID_PAL_MFG_STORE_VERSION, NULL, SID_PAL_MFG_STORE_VERSION_SIZE));

	mfg_set_version(MFG_VERSION_1_VAL);
}

void test_sid_pal_mfg_storage_read(void)
{
	uint32_t version = MFG_VERSION_1_VAL;
	uint8_t read_buff[64];

	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_VERSION, read_buff, SID_PAL_MFG_STORE_VERSION_SIZE);
	TEST_ASSERT_EQUAL_MEMORY(&version, read_buff, SID_PAL_MFG_STORE_VERSION_SIZE);
}

void test_sid_pal_mfg_storage_erase(void)
{
	TEST_ASSERT_FALSE(sid_pal_mfg_store_is_empty());
	mfg_clr_memory();
}

void test_sid_pal_mfg_storage_dev_id_get(void)
{
	uint8_t dev_id[SID_PAL_MFG_STORE_DEVID_SIZE];
	uint8_t fake_dev_id[8] = { 0xAC, 0xBC, 0xCC, 0xDC,
				   0x11, 0x12, 0x13, 0x14 };

	memset(dev_id, 0x00, sizeof(dev_id));
	mfg_clr_memory();

	TEST_ASSERT_FALSE(sid_pal_mfg_store_dev_id_get(dev_id));
	TEST_ASSERT_EQUAL(0xBF, dev_id[0]);
	// dev id is unique for every chip, so we cannot verify it in another way:
	TEST_ASSERT_NOT_EQUAL(0x00, dev_id[1]);
	TEST_ASSERT_NOT_EQUAL(0x00, dev_id[2]);
	TEST_ASSERT_NOT_EQUAL(0x00, dev_id[3]);
	TEST_ASSERT_NOT_EQUAL(0x00, dev_id[4]);

	memset(dev_id, 0x00, sizeof(dev_id));
	// Set fake dev_id.
	TEST_ASSERT_EQUAL(SID_ERROR_NOT_FOUND,
			  sid_pal_mfg_store_write(SID_PAL_MFG_STORE_DEVID, fake_dev_id, sizeof(fake_dev_id)));
}

void test_sid_pal_mfg_storage_sn_get(void)
{
	uint8_t serial_num[SID_PAL_MFG_STORE_SERIAL_NUM_SIZE];
	uint8_t fake_serial_num[20];

	for (int cnt = 0; cnt < sizeof(fake_serial_num); cnt++) {
		fake_serial_num[cnt] = cnt;
	}

	mfg_clr_memory();

	// No serial number.
	TEST_ASSERT_FALSE(sid_pal_mfg_store_serial_num_get(serial_num));

	// Set fake serial number.
	TEST_ASSERT_EQUAL(0,
			  sid_pal_mfg_store_write(SID_PAL_MFG_STORE_SERIAL_NUM, fake_serial_num,
						  sizeof(fake_serial_num)));
	TEST_ASSERT_TRUE(sid_pal_mfg_store_serial_num_get(serial_num));
	TEST_ASSERT_EQUAL_HEX8_ARRAY(fake_serial_num, serial_num, SID_PAL_MFG_STORE_SERIAL_NUM_SIZE);

	// Set version to MFG_VERSION_1_VAL
	mfg_set_version(MFG_VERSION_1_VAL);
	TEST_ASSERT_TRUE(sid_pal_mfg_store_serial_num_get(serial_num));
	hton_buff(serial_num, SID_PAL_MFG_STORE_SERIAL_NUM_SIZE);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(fake_serial_num, serial_num, SID_PAL_MFG_STORE_SERIAL_NUM_SIZE);
}

extern int unity_main(void);

void main(void)
{
	(void)unity_main();
}
