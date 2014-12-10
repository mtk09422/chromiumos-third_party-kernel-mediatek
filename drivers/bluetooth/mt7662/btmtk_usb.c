
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include "btmtk_usb.h"


/* Local Configuration ========================================================= */
#define VERSION "2.0.18"
#define LOAD_PROFILE 1
#define SUPPORT_BT_ATE 1
#define BT_REDUCE_EP2_POLLING_INTERVAL_BY_INTR_TRANSFER 0
#define BT_ROM_PATCH_FROM_BIN 0
#define BT_SEND_HCI_CMD_BEFORE_SUSPEND 1
#define SUPPORT_FW_DUMP 1
#define SUPPORT_FAILED_RESET 0
#define SUPPORT_UNPREDICTED_HCI_CMD_FILTER 1
#define SUPPORT_SEND_DUMMY_BULK_OUT_AFTER_RESUME 1
/* ============================================================================= */

#if BT_ROM_PATCH_FROM_BIN
static int LOAD_CODE_METHOD = BIN_FILE_METHOD;
static unsigned char *mt7662_rom_patch;
#else
static int LOAD_CODE_METHOD = HEADER_METHOD;
#include "./mt7662_rom_patch.h"
#endif


static struct usb_driver btmtk_usb_driver;

static char driver_version[64] = { 0 };
static char rom_patch_version[64] = { 0 };
static char fw_version[64] = { 0 };

static unsigned char probe_counter;

void btmtk_usb_toggle_rst_pin(void)
{
	pr_warn("btmtk_usb_toggle_rst_pin() invoked. No action due to GPIO not defined.\n");
}

static int btmtk_usb_reset(struct usb_device *udev)
{
	int ret;

	pr_warn("%s\n", __func__);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x01, DEVICE_VENDOR_REQUEST_OUT,
			      0x01, 0x00, NULL, 0x00, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d)\n", __func__, ret);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btmtk_usb_io_read32(struct btmtk_usb_data *data, u32 reg, u32 *val)
{
	u8 request = data->r_request;
	struct usb_device *udev = data->udev;
	int ret;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_IN,
			      0x0, reg, data->io_buf, 4, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		*val = 0xffffffff;
		pr_err("%s error(%d), reg=%x, value=%x\n", __func__, ret, reg, *val);
		return ret;
	}

	memmove(val, data->io_buf, 4);

	*val = le32_to_cpu(*val);

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btmtk_usb_io_write32(struct btmtk_usb_data *data, u32 reg, u32 val)
{
	u16 value, index;
	u8 request = data->w_request;
	struct usb_device *udev = data->udev;
	int ret;

	index = (u16) reg;
	value = val & 0x0000ffff;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_OUT,
			      value, index, NULL, 0, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d), reg=%x, value=%x\n", __func__, ret, reg, val);
		return ret;
	}

	index = (u16) (reg + 2);
	value = (val & 0xffff0000) >> 16;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_OUT,
			      value, index, NULL, 0, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d), reg=%x, value=%x\n", __func__, ret, reg, val);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}


#if SUPPORT_FW_DUMP
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/kthread.h>

/* #define FW_DUMP_FILE_NAME "/data/@btmtk/bt_fw_dump" */
#define FW_DUMP_FILE_NAME "/tmp/bt_fw_dump"
#define FW_DUMP_BUF_SIZE (1024*64)
static struct file *fw_dump_file;
static char fw_dump_file_name[64] = { 0 };

int fw_dump_total_read_size = 0;
int fw_dump_total_write_size = 0;
int fw_dump_buffer_used_size = 0;
int fw_dump_buffer_full = 0;
int fw_dump_task_should_stop = 0;
u8 *fw_dump_ptr = NULL;
u8 *fw_dump_read_ptr = NULL;
u8 *fw_dump_write_ptr = NULL;
struct timeval fw_dump_last_write_time;

int fw_dump_end_checking_task_should_stop = 0;
struct urb *fw_dump_bulk_urb = NULL;

static int btmtk_usb_fw_dump_end_checking_thread(void *param)
{
	struct timeval t;

	while (1) {
		mdelay(1000);

		if (kthread_should_stop() || fw_dump_end_checking_task_should_stop)
			return -ERESTARTSYS;

		pr_warn("%s : FW dump on going ... %d:%d\n", "btmtk_usb", fw_dump_total_read_size,
		       fw_dump_total_write_size);

		do_gettimeofday(&t);
		if ((t.tv_sec - fw_dump_last_write_time.tv_sec) > 1) {
			pr_warn("%s : fw dump total read size = %d\n", "btmtk_usb",
			       fw_dump_total_read_size);
			pr_warn("%s : fw dump total write size = %d\n", "btmtk_usb",
			       fw_dump_total_write_size);
			if (fw_dump_file) {
				pr_warn("%s : close file  %s\n", __func__, fw_dump_file_name);
				filp_close(fw_dump_file, NULL);
				fw_dump_file = NULL;
			}
			fw_dump_end_checking_task_should_stop = 1;
			btmtk_usb_toggle_rst_pin();
		}
	}
	return 0;
}

static int btmtk_usb_fw_dump_thread(void *param)
{
	struct btmtk_usb_data *data = param;
	mm_segment_t old_fs;
	int len;

	/*while (down_interruptible(&data->fw_dump_semaphore) == 0) {*/
	while (!kthread_should_stop() && fw_dump_task_should_stop == 0) {
		wait_for_completion(&data->fw_completion);

		len = fw_dump_write_ptr - fw_dump_read_ptr;

		if (len > 0 && fw_dump_read_ptr != NULL) {
			old_fs = get_fs();
			set_fs(KERNEL_DS);

			if (fw_dump_file == NULL) {
				memset(fw_dump_file_name, 0, sizeof(fw_dump_file_name));
				snprintf(fw_dump_file_name, sizeof(fw_dump_file_name),
					 FW_DUMP_FILE_NAME "_%d", probe_counter);
				pr_warn("%s : open file %s\n", __func__, fw_dump_file_name);
				fw_dump_file = filp_open(fw_dump_file_name, O_RDWR | O_CREAT, 0644);
				if (IS_ERR(fw_dump_file)) {
					pr_err("%s : error occured while opening file %s.\n",
					       __func__, fw_dump_file_name);
					set_fs(old_fs);
					return 0;
				}
			}

			if (fw_dump_file != NULL) {
				fw_dump_file->f_op->write(fw_dump_file, fw_dump_read_ptr, len,
							  &fw_dump_file->f_pos);
				fw_dump_read_ptr += len;
				fw_dump_total_write_size += len;
				do_gettimeofday(&fw_dump_last_write_time);
			}
			set_fs(old_fs);
		}

		if (fw_dump_buffer_full && fw_dump_write_ptr == fw_dump_read_ptr) {
			int err;

			fw_dump_buffer_full = 0;
			fw_dump_buffer_used_size = 0;
			fw_dump_read_ptr = fw_dump_ptr;
			fw_dump_write_ptr = fw_dump_ptr;

			err = usb_submit_urb(fw_dump_bulk_urb, GFP_ATOMIC);
			if (err < 0) {
				/* -EPERM: urb is being killed;
				 * -ENODEV: device got disconnected */
				if (err != -EPERM && err != -ENODEV)
					pr_err("urb %p failed to resubmit bulk_in_urb(%d)",
					       fw_dump_bulk_urb, -err);
				usb_unanchor_urb(fw_dump_bulk_urb);
			}
		}

		if (data->fw_dump_end_check_tsk == NULL) {
			fw_dump_end_checking_task_should_stop = 0;
			data->fw_dump_end_check_tsk =
			    kthread_create(btmtk_usb_fw_dump_end_checking_thread, (void *)data,
					   "btmtk_usb_fw_dump_end_checking_thread");
			if (IS_ERR(data->fw_dump_end_check_tsk)) {
				pr_err("%s : create fw dump end check thread failed!\n", __func__);
				data->fw_dump_end_check_tsk = NULL;
			} else {
				wake_up_process(data->fw_dump_end_check_tsk);
			}
		}
	}

	if (kthread_should_stop() || fw_dump_task_should_stop) {
		if (fw_dump_file) {
			pr_warn("%s : close file  %s\n", __func__, fw_dump_file_name);
			filp_close(fw_dump_file, NULL);
			fw_dump_file = NULL;
			pr_warn("%s : fw dump total read size = %d\n", "btmtk_usb",
				fw_dump_total_read_size);
			pr_warn("%s : fw dump total write size = %d\n", "btmtk_usb",
				fw_dump_total_write_size);
			btmtk_usb_toggle_rst_pin();
		}
		return -ERESTARTSYS;
	}

	pr_warn("%s end : down != 0\n", __func__);

	return 0;
}
#endif

static void send_dummy_bulk_out_packet_complete(struct urb *urb)
{
}

static int btmtk_usb_send_dummy_bulk_out_packet(struct btmtk_usb_data *data)
{
	int ret = 0;
	unsigned char buf[8] = { 0 };
	struct urb *urb;
	unsigned int pipe;

	pr_warn("%s\n", __func__);
	pipe = usb_sndbulkpipe(data->udev, data->bulk_tx_ep->bEndpointAddress);

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		pr_err("%s: usb_alloc_usb failed!\n", __func__);
		goto error0;
	}
	usb_fill_bulk_urb(urb,
			  data->udev,
			  pipe, buf, sizeof(buf), send_dummy_bulk_out_packet_complete, NULL);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret)
		pr_err("%s: submit urb failed!\n", __func__);

	usb_free_urb(urb);
 error0:
	return ret;
}

static int btmtk_usb_send_hci_suspend_cmd(struct usb_device *udev)
{
	int ret = 0;
	char buf[5] = { 0 };

	buf[0] = 0xc9;
	buf[1] = 0xfc;
	buf[2] = 0x02;
	buf[3] = 0x01;
	buf[4] = 0x0a;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
			      0x00, 0x00, buf, 0x05, 10);

	if (ret < 0) {
		pr_err("%s error1(%d)\n", __func__, ret);
		return ret;
	}
	pr_warn("btmtk_usb_send_hci_suspend_cmd : OK\n");

	return 0;
}

static int btmtk_usb_send_hci_reset_cmd(struct usb_device *udev)
{
	int retry_counter = 0;
	/* Send HCI Reset */
	{
		int ret = 0;
		char buf[4] = { 0 };

		buf[0] = 0x03;
		buf[1] = 0x0C;
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x03, 10);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		while (1) {
			int ret = 0;
			char buf[64] = { 0 };
			int actual_length;

			ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
						buf, 64, &actual_length, 2000);

			if (ret < 0) {
				pr_err("%s error2(%d)\n", __func__, ret);
				return ret;
			}

			if (actual_length == 6 &&
			    buf[0] == 0x0e &&
			    buf[1] == 0x04 &&
			    buf[2] == 0x01 && buf[3] == 0x03 && buf[4] == 0x0c && buf[5] == 0x00) {
				break;
			} else {
				int i;

				pr_err("%s drop unknown event :\n", __func__);
				for (i = 0; i < actual_length && i < 64; i++)
					pr_err("%02X ", buf[i]);

				pr_err("\n");
				mdelay(10);
				retry_counter++;
			}

			if (retry_counter > 10) {
				pr_err("%s retry timeout!\n", __func__);
				return ret;
			}
		}
	}

	pr_warn("btmtk_usb_send_hci_reset_cmd : OK\n");
	return 0;
}

static int btmtk_usb_send_hci_set_ce_cmd(struct usb_device *udev)
{
	char result_buf[64] = { 0 };

	/* Send HCI Reset, read 0x41070c */
	{
		int ret = 0;
		char buf[8] = { 0xd1, 0xFC, 0x04, 0x0c, 0x07, 0x41, 0x00 };

		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x07, 10);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		int ret = 0;
		int actual_length;

		ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
					result_buf, 64, &actual_length, 2000);

		if (ret < 0) {
			pr_err("%s error2(%d)\n", __func__, ret);
			return ret;
		} else {
			if (result_buf[6] & 0x01)
				pr_warn("warning, 0x41070c[0] is 1!\n");
		}
	}

	/* Send HCI Reset, write 0x41070c[0] to 1 */
	{
		int ret = 0;
		char buf[12] = { 0xd0, 0xfc, 0x08, 0x0c, 0x07, 0x41, 0x00 };

		buf[7] = result_buf[6] | 0x01;
		buf[8] = result_buf[7];
		buf[9] = result_buf[8];
		buf[10] = result_buf[9];
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x0b, 10);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	pr_warn("btmtk_usb_send_hci_set_ce_cmd : OK\n");
	return 0;
}


static int btmtk_usb_send_check_rom_patch_result_cmd(struct usb_device *udev)
{
	/* Send HCI Reset */
	{
		int ret = 0;
		unsigned char buf[8] = { 0 };

		buf[0] = 0xD1;
		buf[1] = 0xFC;
		buf[2] = 0x04;
		buf[3] = 0x00;
		buf[4] = 0xE2;
		buf[5] = 0x40;
		buf[6] = 0x00;
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x07, 10);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		int ret = 0;
		unsigned char buf[64] = { 0 };
		int actual_length;

		ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
					buf, 64, &actual_length, 2000);

		if (ret < 0) {
			pr_err("%s error2(%d)\n", __func__, ret);
			return ret;
		}
		pr_warn("Check rom patch result : ");

		if (buf[6] == 0 && buf[7] == 0 && buf[8] == 0 && buf[9] == 0)
			pr_err("NG\n");
		else
			pr_warn("OK\n");
	}

	return 0;
}

static int btmtk_usb_switch_iobase(struct btmtk_usb_data *data, int base)
{
	int ret = 0;

	switch (base) {
	case SYSCTL:
		data->w_request = 0x42;
		data->r_request = 0x47;
		break;
	case WLAN:
		data->w_request = 0x02;
		data->r_request = 0x07;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static void btmtk_usb_cap_init(struct btmtk_usb_data *data)
{
	btmtk_usb_io_read32(data, 0x00, &data->chip_id);

	pr_warn("chip id = %x\n", data->chip_id);

	if (is_mt7630(data) || is_mt7650(data)) {
		data->need_load_fw = 1;
		data->need_load_rom_patch = 0;
		data->fw_header_image = NULL;
		data->fw_bin_file_name = "mtk/mt7650.bin";
		data->fw_len = 0;
	} else if (is_mt7632(data) || is_mt7662(data)) {
		data->need_load_fw = 0;
		data->need_load_rom_patch = 1;
		if (LOAD_CODE_METHOD == HEADER_METHOD) {
			data->rom_patch_header_image = mt7662_patch;
			data->rom_patch_len = sizeof(mt7662_patch);
			data->rom_patch_offset = 0x90000;
		} else {
			data->rom_patch_bin_file_name = kmalloc(32, GFP_ATOMIC);
			if (!data->rom_patch_bin_file_name) {
				pr_err("btmtk_usb_cap_init(): Can't allocate memory (32)\n");
				return;
			}
			memset(data->rom_patch_bin_file_name, 0, 32);

			if ((data->chip_id & 0xf) < 0x2)
				memcpy(data->rom_patch_bin_file_name, "mt7662_patch_e1_hdr.bin",
				       23);
			else
				memcpy(data->rom_patch_bin_file_name, "mt7662_patch_e3_hdr.bin",
				       23);

			data->rom_patch_offset = 0x90000;
			data->rom_patch_len = 0;
		}
	} else {
		pr_warn("unknow chip(%x)\n", data->chip_id);
	}
}

u16 checksume16(u8 *pData, int len)
{
	int sum = 0;

	while (len > 1) {
		sum += *((u16 *) pData);

		pData = pData + 2;

		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);

		len -= 2;
	}

	if (len)
		sum += *((u8 *) pData);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

static int btmtk_usb_chk_crc(struct btmtk_usb_data *data, u32 checksum_len)
{
	int ret = 0;
	struct usb_device *udev = data->udev;

	pr_warn("%s", __func__);

	memmove(data->io_buf, &data->rom_patch_offset, 4);
	memmove(&data->io_buf[4], &checksum_len, 4);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x1, DEVICE_VENDOR_REQUEST_OUT,
			      0x20, 0x00, data->io_buf, 8, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static u16 btmtk_usb_get_crc(struct btmtk_usb_data *data)
{
	int ret = 0;
	struct usb_device *udev = data->udev;
	u16 crc, count = 0;

	pr_warn("%s", __func__);

	while (1) {
		ret =
		    usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x01, DEVICE_VENDOR_REQUEST_IN,
				    0x21, 0x00, data->io_buf, 2, CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			crc = 0xFFFF;
			pr_err("%s error(%d)\n", __func__, ret);
		}

		memmove(&crc, data->io_buf, 2);

		crc = le16_to_cpu(crc);

		if (crc != 0xFFFF)
			break;

		mdelay(100);

		if (count++ > 100) {
			pr_err("Query CRC over %d times\n", count);
			break;
		}
	}

	return crc;
}

static int btmtk_usb_reset_wmt(struct btmtk_usb_data *data)
{
	int ret = 0;

	/* reset command */
	u8 cmd[9] = { 0x6F, 0xFC, 0x05, 0x01, 0x07, 0x01, 0x00, 0x04 };

	memmove(data->io_buf, cmd, 8);

	ret = usb_control_msg(data->udev, usb_sndctrlpipe(data->udev, 0), 0x01,
			      DEVICE_CLASS_REQUEST_OUT, 0x30, 0x00, data->io_buf, 8,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s:Err1(%d)\n", __func__, ret);
		return ret;
	}

	mdelay(20);

	ret = usb_control_msg(data->udev, usb_rcvctrlpipe(data->udev, 0), 0x01,
			      DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00, data->io_buf, 7,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s Err2(%d)\n", __func__, ret);

	if (data->io_buf[0] == 0xe4 &&
	    data->io_buf[1] == 0x05 &&
	    data->io_buf[2] == 0x02 &&
	    data->io_buf[3] == 0x07 &&
	    data->io_buf[4] == 0x01 && data->io_buf[5] == 0x00 && data->io_buf[6] == 0x00) {
		pr_warn("Check reset wmt result : OK\n");
	} else {
		pr_err("Check reset wmt result : NG\n");
	}


	return ret;
}

static u16 btmtk_usb_get_rom_patch_result(struct btmtk_usb_data *data)
{
	int ret = 0;

	ret = usb_control_msg(data->udev, usb_rcvctrlpipe(data->udev, 0), 0x01,
			      DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00, data->io_buf, 7,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	if (data->io_buf[0] == 0xe4 &&
	    data->io_buf[1] == 0x05 &&
	    data->io_buf[2] == 0x02 &&
	    data->io_buf[3] == 0x01 &&
	    data->io_buf[4] == 0x01 && data->io_buf[5] == 0x00 && data->io_buf[6] == 0x00) {
		pr_warn("Get rom patch result : OK\n");
	} else {
		pr_err("Get rom patch result : NG\n");
	}

	return ret;
}

static void load_code_from_bin(unsigned char **image, char *bin_name, struct device *dev,
			       u32 *code_len)
{
	const struct firmware *fw_entry;

	if (request_firmware(&fw_entry, bin_name, dev) != 0) {
		*image = NULL;
		return;
	}

	*image = kmalloc(fw_entry->size, GFP_ATOMIC);
	memcpy(*image, fw_entry->data, fw_entry->size);
	*code_len = fw_entry->size;

	release_firmware(fw_entry);
}

static void load_rom_patch_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btmtk_usb_load_rom_patch(struct btmtk_usb_data *data)
{
	u32 loop = 0;
	u32 value;
	s32 sent_len;
	int ret = 0, total_checksum = 0;
	struct urb *urb;
	u32 patch_len = 0;
	u32 cur_len = 0;
	dma_addr_t data_dma;
	struct completion sent_to_mcu_done;
	int first_block = 1;
	unsigned char phase;
	void *buf;
	char *pos;
	char *tmp_str;
	unsigned int pipe = usb_sndbulkpipe(data->udev,
					    data->bulk_tx_ep->bEndpointAddress);

	pr_warn("btmtk_usb_load_rom_patch begin\n");
 load_patch_protect:
	btmtk_usb_switch_iobase(data, WLAN);
	btmtk_usb_io_read32(data, SEMAPHORE_03, &value);
	loop++;

	if ((value & 0x01) == 0x00) {
		if (loop < 1000) {
			mdelay(1);
			goto load_patch_protect;
		} else {
			pr_err("btmtk_usb_load_rom_patch ERR! Can't get semaphore! Continue\n");
		}
	}

	btmtk_usb_switch_iobase(data, SYSCTL);

	btmtk_usb_io_write32(data, 0x1c, 0x30);

	btmtk_usb_switch_iobase(data, WLAN);

	/* check ROM patch if upgrade */
	if ((MT_REV_GTE(data, mt7662, REV_MT76x2E3)) || (MT_REV_GTE(data, mt7632, REV_MT76x2E3))) {
		btmtk_usb_io_read32(data, CLOCK_CTL, &value);
		if ((value & 0x01) == 0x01) {
			pr_warn("btmtk_usb_load_rom_patch : no need to load rom patch\n");
			btmtk_usb_send_hci_reset_cmd(data->udev);
#ifdef SUPPORT_SEND_DUMMY_BULK_OUT_AFTER_RESUME
			btmtk_usb_send_dummy_bulk_out_packet(data);
			btmtk_usb_send_dummy_bulk_out_packet(data);
#endif
			goto error0;
		}
	} else {
		btmtk_usb_io_read32(data, COM_REG0, &value);
		if ((value & 0x02) == 0x02) {
			pr_warn("btmtk_usb_load_rom_patch : no need to load rom patch\n");
			btmtk_usb_send_hci_reset_cmd(data->udev);
#ifdef SUPPORT_SEND_DUMMY_BULK_OUT_AFTER_RESUME
			btmtk_usb_send_dummy_bulk_out_packet(data);
			btmtk_usb_send_dummy_bulk_out_packet(data);
#endif
			goto error0;
		}
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error0;
	}

	buf = usb_alloc_coherent(data->udev, UPLOAD_PATCH_UNIT, GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error1;
	}

	pos = buf;

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		load_code_from_bin(&data->rom_patch, data->rom_patch_bin_file_name,
				   &data->udev->dev, &data->rom_patch_len);
		pr_warn("LOAD_CODE_METHOD : BIN_FILE_METHOD\n");
	} else {
		data->rom_patch = data->rom_patch_header_image;
		pr_warn("LOAD_CODE_METHOD : HEADER_METHOD\n");
	}

	if (!data->rom_patch) {
		if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
			pr_err
			    ("%s:please assign a rom patch(/etc/firmware/%s)or(/lib/firmware/%s)\n",
			     __func__, data->rom_patch_bin_file_name,
			     data->rom_patch_bin_file_name);
		} else {
			pr_err("%s:please assign a rom patch\n", __func__);
		}

		ret = -1;
		goto error2;
	}

	tmp_str = data->rom_patch;
	pr_warn("FW Version = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	       tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3],
	       tmp_str[4], tmp_str[5], tmp_str[6], tmp_str[7],
	       tmp_str[8], tmp_str[9], tmp_str[10], tmp_str[11],
	       tmp_str[12], tmp_str[13], tmp_str[14], tmp_str[15]);

	pr_warn("build Time = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	       tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3],
	       tmp_str[4], tmp_str[5], tmp_str[6], tmp_str[7],
	       tmp_str[8], tmp_str[9], tmp_str[10], tmp_str[11],
	       tmp_str[12], tmp_str[13], tmp_str[14], tmp_str[15]);

	memset(rom_patch_version, 0, sizeof(rom_patch_version));
	memcpy(rom_patch_version, tmp_str, 15);

	tmp_str = data->rom_patch + 16;
	pr_warn("platform = %c%c%c%c\n", tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);


	tmp_str = data->rom_patch + 20;
	pr_warn("HW/SW version = %c%c%c%c\n", tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	tmp_str = data->rom_patch + 24;
	pr_warn("Patch version = %c%c%c%c\n", tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);


	pr_warn("\nloading rom patch...\n");

	init_completion(&sent_to_mcu_done);

	cur_len = 0x00;
	patch_len = data->rom_patch_len - PATCH_INFO_SIZE;

	/* loading rom patch */
	while (1) {
		s32 sent_len_max = UPLOAD_PATCH_UNIT - PATCH_HEADER_SIZE;

		sent_len =
		    (patch_len - cur_len) >= sent_len_max ? sent_len_max : (patch_len - cur_len);

		pr_warn("patch_len = %d\n", patch_len);
		pr_warn("cur_len = %d\n", cur_len);
		pr_warn("sent_len = %d\n", sent_len);

		if (sent_len > 0) {
			if (first_block == 1) {
				if (sent_len < sent_len_max)
					phase = PATCH_PHASE3;
				else
					phase = PATCH_PHASE1;
				first_block = 0;
			} else if (sent_len == sent_len_max) {
				phase = PATCH_PHASE2;
			} else {
				phase = PATCH_PHASE3;
			}

			/* prepare HCI header */
			pos[0] = 0x6F;
			pos[1] = 0xFC;
			pos[2] = (sent_len + 5) & 0xFF;
			pos[3] = ((sent_len + 5) >> 8) & 0xFF;

			/* prepare WMT header */
			pos[4] = 0x01;
			pos[5] = 0x01;
			pos[6] = (sent_len + 1) & 0xFF;
			pos[7] = ((sent_len + 1) >> 8) & 0xFF;

			pos[8] = phase;

			memcpy(&pos[9], data->rom_patch + PATCH_INFO_SIZE + cur_len, sent_len);

			pr_warn("sent_len + PATCH_HEADER_SIZE = %d, phase = %d\n",
			       sent_len + PATCH_HEADER_SIZE, phase);

			usb_fill_bulk_urb(urb,
					  data->udev,
					  pipe,
					  buf,
					  sent_len + PATCH_HEADER_SIZE,
					  load_rom_patch_complete, &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error2;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				pr_err("upload rom_patch timeout\n");
				goto error2;
			}

			mdelay(1);

			cur_len += sent_len;

		} else {
			break;
		}
	}

	mdelay(20);
	ret = btmtk_usb_get_rom_patch_result(data);
	mdelay(20);

	/* Send Checksum request */
	total_checksum = checksume16(data->rom_patch + PATCH_INFO_SIZE, patch_len);
	btmtk_usb_chk_crc(data, patch_len);

	mdelay(20);

	if (total_checksum != btmtk_usb_get_crc(data)) {
		pr_err("checksum fail!, local(0x%x) <> fw(0x%x)\n", total_checksum,
		       btmtk_usb_get_crc(data));
		ret = -1;
		goto error2;
	}

	mdelay(20);
	/* send check rom patch result request */
	btmtk_usb_send_check_rom_patch_result_cmd(data->udev);
	mdelay(20);
	/* CHIP_RESET */
	ret = btmtk_usb_reset_wmt(data);
	mdelay(20);
	/* BT_RESET */
	btmtk_usb_send_hci_reset_cmd(data->udev);

	/* for WoBLE/WoW low power */
	btmtk_usb_send_hci_set_ce_cmd(data->udev);

 error2:
	usb_free_coherent(data->udev, UPLOAD_PATCH_UNIT, buf, data_dma);
 error1:
	usb_free_urb(urb);
 error0:
	btmtk_usb_io_write32(data, SEMAPHORE_03, 0x1);
	pr_warn("btmtk_usb_load_rom_patch end\n");
	return ret;
}


static int load_fw_iv(struct btmtk_usb_data *data)
{
	int ret;
	struct usb_device *udev = data->udev;
	char *buf = kmalloc(64, GFP_ATOMIC);

	memmove(buf, data->fw_image + 32, 64);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x01,
			      DEVICE_VENDOR_REQUEST_OUT, 0x12, 0x0, buf, 64,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d) step4\n", __func__, ret);
		kfree(buf);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	kfree(buf);

	return ret;
}

static void load_fw_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btmtk_usb_load_fw(struct btmtk_usb_data *data)
{
	struct usb_device *udev = data->udev;
	struct urb *urb;
	void *buf;
	u32 cur_len = 0;
	u32 packet_header = 0;
	u32 value;
	u32 ilm_len = 0, dlm_len = 0;
	u16 fw_ver, build_ver;
	u32 loop = 0;
	dma_addr_t data_dma;
	int ret = 0, sent_len;
	struct completion sent_to_mcu_done;
	unsigned int pipe = usb_sndbulkpipe(data->udev,
					    data->bulk_tx_ep->bEndpointAddress);

	pr_warn("bulk_tx_ep = %x\n", data->bulk_tx_ep->bEndpointAddress);

 loadfw_protect:
	btmtk_usb_switch_iobase(data, WLAN);
	btmtk_usb_io_read32(data, SEMAPHORE_00, &value);
	loop++;

	if (((value & 0x1) == 0) && (loop < 10000))
		goto loadfw_protect;

	/* check MCU if ready */
	btmtk_usb_io_read32(data, COM_REG0, &value);

	if ((value & 0x01) == 0x01)
		goto error0;

	/* Enable MPDMA TX and EP2 load FW mode */
	btmtk_usb_io_write32(data, 0x238, 0x1c000000);

	btmtk_usb_reset(udev);
	mdelay(100);

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		load_code_from_bin(&data->fw_image, data->fw_bin_file_name, &data->udev->dev,
				   &data->fw_len);
		pr_warn("LOAD_CODE_METHOD : BIN_FILE_METHOD\n");
	} else {
		data->fw_image = data->fw_header_image;
		pr_warn("LOAD_CODE_METHOD : HEADER_METHOD\n");
	}

	if (!data->fw_image) {
		if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
			pr_warn("%s:please assign a fw(/etc/firmware/%s)or(/lib/firmware/%s)\n",
			       __func__, data->fw_bin_file_name, data->fw_bin_file_name);
		} else {
			pr_warn("%s:please assign a fw\n", __func__);
		}

		ret = -1;
		goto error0;
	}

	ilm_len = (*(data->fw_image + 3) << 24) | (*(data->fw_image + 2) << 16) |
	    (*(data->fw_image + 1) << 8) | (*data->fw_image);

	dlm_len = (*(data->fw_image + 7) << 24) | (*(data->fw_image + 6) << 16) |
	    (*(data->fw_image + 5) << 8) | (*(data->fw_image + 4));

	fw_ver = (*(data->fw_image + 11) << 8) | (*(data->fw_image + 10));

	build_ver = (*(data->fw_image + 9) << 8) | (*(data->fw_image + 8));

	pr_warn("fw version:%d.%d.%02d ", (fw_ver & 0xf000) >> 8,
	       (fw_ver & 0x0f00) >> 8, (fw_ver & 0x00ff));

	pr_warn("build:%x\n", build_ver);

	pr_warn("build Time =");

	for (loop = 0; loop < 16; loop++)
		pr_warn("%c", *(data->fw_image + 16 + loop));

	pr_warn("\n");
	pr_warn("ILM length = %d(bytes)\n", ilm_len);
	pr_warn("DLM length = %d(bytes)\n", dlm_len);

	btmtk_usb_switch_iobase(data, SYSCTL);

	/* U2M_PDMA rx_ring_base_ptr */
	btmtk_usb_io_write32(data, 0x790, 0x400230);

	/* U2M_PDMA rx_ring_max_cnt */
	btmtk_usb_io_write32(data, 0x794, 0x1);

	/* U2M_PDMA cpu_idx */
	btmtk_usb_io_write32(data, 0x798, 0x1);

	/* U2M_PDMA enable */
	btmtk_usb_io_write32(data, 0x704, 0x44);

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error1;
	}

	buf = usb_alloc_coherent(udev, 14592, GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error2;
	}

	BT_DBG("loading fw");

	init_completion(&sent_to_mcu_done);

	btmtk_usb_switch_iobase(data, SYSCTL);

	cur_len = 0x40;

	/* Loading ILM */
	while (1) {
		sent_len = (ilm_len - cur_len) >= 14336 ? 14336 : (ilm_len - cur_len);

		if (sent_len > 0) {
			packet_header &= ~(0xffffffff);
			packet_header |= (sent_len << 16);
			packet_header = cpu_to_le32(packet_header);

			memmove(buf, &packet_header, 4);
			memmove(buf + 4, data->fw_image + 32 + cur_len, sent_len);

			/* U2M_PDMA descriptor */
			btmtk_usb_io_write32(data, 0x230, cur_len);

			while ((sent_len % 4) != 0)
				sent_len++;

			/* U2M_PDMA length */
			btmtk_usb_io_write32(data, 0x234, sent_len << 16);

			usb_fill_bulk_urb(urb,
					  udev,
					  pipe,
					  buf, sent_len + 4, load_fw_complete, &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error3;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				pr_err("upload ilm fw timeout\n");
				goto error3;
			}

			mdelay(200);

			cur_len += sent_len;
		} else {
			break;
		}
	}

	init_completion(&sent_to_mcu_done);
	cur_len = 0x00;

	/* Loading DLM */
	while (1) {
		sent_len = (dlm_len - cur_len) >= 14336 ? 14336 : (dlm_len - cur_len);

		if (sent_len > 0) {
			packet_header &= ~(0xffffffff);
			packet_header |= (sent_len << 16);
			packet_header = cpu_to_le32(packet_header);

			memmove(buf, &packet_header, 4);
			memmove(buf + 4, data->fw_image + 32 + ilm_len + cur_len, sent_len);

			/* U2M_PDMA descriptor */
			btmtk_usb_io_write32(data, 0x230, 0x80000 + cur_len);

			while ((sent_len % 4) != 0) {
				pr_err("sent_len is not divided by 4\n");
				sent_len++;
			}

			/* U2M_PDMA length */
			btmtk_usb_io_write32(data, 0x234, sent_len << 16);

			usb_fill_bulk_urb(urb,
					  udev,
					  pipe,
					  buf, sent_len + 4, load_fw_complete, &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error3;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				pr_err("upload dlm fw timeout\n");
				goto error3;
			}

			mdelay(500);

			cur_len += sent_len;
		} else {
			break;
		}
	}

	/* upload 64bytes interrupt vector */
	ret = load_fw_iv(data);
	mdelay(100);

	btmtk_usb_switch_iobase(data, WLAN);

	/* check MCU if ready */
	loop = 0;

	do {
		btmtk_usb_io_read32(data, COM_REG0, &value);

		if (value == 0x01)
			break;

		mdelay(10);
		loop++;
	} while (loop <= 100);

	if (loop > 1000) {
		pr_warn("wait for 100 times\n");
		ret = -ENODEV;
	}

 error3:
	usb_free_coherent(udev, 14592, buf, data_dma);
 error2:
	usb_free_urb(urb);
 error1:
	/* Disbale load fw mode */
	btmtk_usb_io_read32(data, 0x238, &value);
	value = value & ~(0x10000000);
	btmtk_usb_io_write32(data, 0x238, value);
 error0:
	btmtk_usb_io_write32(data, SEMAPHORE_00, 0x1);
	return ret;
}

static int inc_tx(struct btmtk_usb_data *data)
{
	unsigned long flags;
	int rv;

	spin_lock_irqsave(&data->txlock, flags);
	rv = test_bit(BTUSB_SUSPENDING, &data->flags);
	if (!rv)
		data->tx_in_flight++;
	spin_unlock_irqrestore(&data->txlock, flags);

	return rv;
}

static void btmtk_usb_intr_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int err;

#if SUPPORT_BT_ATE
	int is_ate_event = 0;
#endif

	pr_info("%s: %s urb %p status %d count %d", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

#if SUPPORT_BT_ATE
		/* check ATE cmd event */
		{
			unsigned char *event_buf = urb->transfer_buffer;
			u32 event_buf_len = urb->actual_length;
			u8 matched = 0;
			int i;
			u32 Count_Tx_ACL = 0;
			u32 Count_ACK = 0;
			u8 PHY_RATE = 0;

			if (event_buf) {
				if (event_buf[3] == 0x4D && event_buf[4] == 0xFC)
					matched = 1;

				if (event_buf[3] == 0x3F && event_buf[4] == 0x0C)
					matched = 2;

				if (matched == 1) {
					is_ate_event = 1;
					pr_warn("Got ATE event result:(%d)\n    ", event_buf_len);
					for (i = 0; i < event_buf_len; i++)
						pr_warn("%02X ", event_buf[i]);

					pr_warn("\n");

					Count_Tx_ACL =
					    event_buf[6] | ((event_buf[7] << 8) & 0xff00) |
					    ((event_buf[8] << 16) & 0xff0000) |
					    ((event_buf[9] << 24) & 0xff000000);
					Count_ACK =
					    event_buf[10] | ((event_buf[11] << 8) & 0xff00) |
					    ((event_buf[12] << 16) & 0xff0000) |
					    ((event_buf[13] << 24) & 0xff000000);
					PHY_RATE = event_buf[14];

					pr_warn("Count_Tx_ACL = 0x%08X\n", Count_Tx_ACL);
					pr_warn("Count_ACK = 0x%08X\n", Count_ACK);
					if (PHY_RATE == 0)
						pr_warn("PHY_RATE = 1M_DM\n");
					else if (PHY_RATE == 1)
						pr_warn("PHY_RATE = 1M_DH\n");
					else if (PHY_RATE == 2)
						pr_warn("PHY_RATE = 2M\n");
					else if (PHY_RATE == 3)
						pr_warn("PHY_RATE = 3M\n");
				} else if (matched == 2) {
					pr_warn
					    ("Got ATE AFH Host Channel Classification Command Result:(%d)\n    ",
					     event_buf_len);
					for (i = 0; i < event_buf_len; i++)
						pr_warn("%02X ", event_buf[i]);

					pr_warn("\n");

					if (event_buf[5] == 0)
						pr_warn("Result: Success\n");
					else
						pr_err("Result: Failed(0x%x)\n", event_buf[5]);

				}
			}
		}

		if (is_ate_event == 0)
#endif				/* SUPPORT_BT_ATE */
			if (hci_recv_fragment(hdev, HCI_EVENT_PKT,
					      urb->transfer_buffer, urb->actual_length) < 0) {
				pr_warn("%s corrupted event packet", hdev->name);
				hdev->stat.err_rx++;
			}
	}

	if (!test_bit(BTUSB_INTR_RUNNING, &data->flags))
		return;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);

	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p failed to resubmit intr_in_urb(%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btmtk_usb_submit_intr_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	pr_warn("%s", __func__);

	if (!data->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->intr_ep->wMaxPacketSize);

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvintpipe(data->udev, data->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size,
			 btmtk_usb_intr_complete, hdev, data->intr_ep->bInterval);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;


}

static void btmtk_usb_bulk_in_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	int err;

	pr_warn("%s:%s urb %p status %d count %d", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

#if SUPPORT_FW_DUMP
	{
		u8 *buf = urb->transfer_buffer;
		u16 len = 0;

		if (urb->actual_length > 4) {
			len = buf[2] + ((buf[3] << 8) & 0xff00);
			if (buf[0] == 0x6f && buf[1] == 0xfc && len + 4 == urb->actual_length) {
				if (fw_dump_buffer_full)
					pr_warn
						("data comes when buffer full!! (Shouldn't Happen!!)\n");

				fw_dump_total_read_size += len;
				if (fw_dump_write_ptr + len < fw_dump_ptr + FW_DUMP_BUF_SIZE) {
					fw_dump_buffer_used_size += len;

					if (fw_dump_buffer_used_size + 512 > FW_DUMP_BUF_SIZE)
						fw_dump_buffer_full = 1;

					memcpy(fw_dump_write_ptr, &buf[4], len);
					fw_dump_write_ptr += len;
					/* up(&data->fw_dump_semaphore); */
					complete(&data->fw_completion);
				} else {
					pr_warn
						("buffer size too small ! (%d:%d) (Shouldn't Happen!!)\n",
						FW_DUMP_BUF_SIZE, fw_dump_total_read_size);
				}

			}
		}
	}
#endif

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

		if (hci_recv_fragment(hdev, HCI_ACLDATA_PKT,
				      urb->transfer_buffer, urb->actual_length) < 0) {
			pr_warn("%s corrupted ACL packet", hdev->name);
			hdev->stat.err_rx++;
		}
	}

	if (!test_bit(BTUSB_BULK_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->bulk_anchor);
	usb_mark_last_busy(data->udev);

#if SUPPORT_FW_DUMP
	if (fw_dump_buffer_full) {
		fw_dump_bulk_urb = urb;
		err = 0;
	} else {
		err = usb_submit_urb(urb, GFP_ATOMIC);
	}
#else
	err = usb_submit_urb(urb, GFP_ATOMIC);
#endif
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p failed to resubmit bulk_in_urb(%d)", hdev->name, urb,
			       -err);
		usb_unanchor_urb(urb);
	}
}

static int btmtk_usb_submit_bulk_in_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size = HCI_MAX_FRAME_SIZE;

	pr_warn("%s:%s", __func__, hdev->name);

	if (!data->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
#if BT_REDUCE_EP2_POLLING_INTERVAL_BY_INTR_TRANSFER
	pipe = usb_rcvintpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);
	/* interval : 1ms */
	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btmtk_usb_bulk_in_complete, hdev, 4);
#else
	pipe = usb_rcvbulkpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);
	usb_fill_bulk_urb(urb, data->udev, pipe, buf, size, btmtk_usb_bulk_in_complete, hdev);
#endif

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->bulk_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btmtk_usb_isoc_in_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int i, err;

	pr_warn("%s: %s urb %p status %d count %d", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned int offset = urb->iso_frame_desc[i].offset;
			unsigned int length = urb->iso_frame_desc[i].actual_length;

			if (urb->iso_frame_desc[i].status)
				continue;

			hdev->stat.byte_rx += length;

			if (hci_recv_fragment(hdev, HCI_SCODATA_PKT,
					      urb->transfer_buffer + offset, length) < 0) {
				pr_warn("%s corrupted SCO packet", hdev->name);
				hdev->stat.err_rx++;
			}
		}
	}

	if (!test_bit(BTUSB_ISOC_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p failed to resubmit iso_in_urb(%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static inline void __fill_isoc_descriptor(struct urb *urb, int len, int mtu)
{
	int i, offset = 0;

	pr_warn("len %d mtu %d", len, mtu);

	for (i = 0; i < BTUSB_MAX_ISOC_FRAMES && len >= mtu; i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
	}

	if (len && i < BTUSB_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		i++;
	}

	urb->number_of_packets = i;
}

static int btmtk_usb_submit_isoc_in_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	pr_warn("%s\n", __func__);

	if (!data->isoc_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize) * BTUSB_MAX_ISOC_FRAMES;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvisocpipe(data->udev, data->isoc_rx_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btmtk_usb_isoc_in_complete,
			 hdev, data->isoc_rx_ep->bInterval);

	urb->transfer_flags = URB_FREE_BUFFER | URB_ISO_ASAP;

	__fill_isoc_descriptor(urb, size, le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize));

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

#if LOAD_PROFILE
static void btmtk_usb_ctrl_complete(struct urb *urb)
{
	pr_warn("btmtk_usb_ctrl_complete\n");
	kfree(urb->setup_packet);
	kfree(urb->transfer_buffer);
}

static int btmtk_usb_submit_ctrl_urb(struct hci_dev *hdev, char *buf, int length)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct usb_ctrlrequest *setup_packet;
	struct urb *urb;
	unsigned int pipe;
	char *send_buf;
	int err;

	pr_warn("btmtk_usb_submit_ctrl_urb, length=%d\n", length);

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		pr_err("btmtk_usb_submit_ctrl_urb error1\n");
		return -ENOMEM;
	}

	send_buf = kmalloc(length, GFP_ATOMIC);
	if (!send_buf) {
		pr_err("btmtk_usb_submit_ctrl_urb error2\n");
		usb_free_urb(urb);
		return -ENOMEM;
	}
	memcpy(send_buf, buf, length);

	setup_packet = kmalloc(sizeof(*setup_packet), GFP_ATOMIC);
	if (!setup_packet) {
		pr_err("btmtk_usb_submit_ctrl_urb error3\n");
		usb_free_urb(urb);
		kfree(send_buf);
		return -ENOMEM;
	}

	setup_packet->bRequestType = data->cmdreq_type;
	setup_packet->bRequest = 0;
	setup_packet->wIndex = 0;
	setup_packet->wValue = 0;
	setup_packet->wLength = __cpu_to_le16(length);

	pipe = usb_sndctrlpipe(data->udev, 0x00);

	usb_fill_control_urb(urb, data->udev, pipe, (void *)setup_packet,
			     send_buf, length, btmtk_usb_ctrl_complete, hdev);

	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

	usb_free_urb(urb);
	return err;
}

int _ascii_to_int(char buf)
{
	switch (buf) {
	case 'a':
	case 'A':
		return 10;
	case 'b':
	case 'B':
		return 11;
	case 'c':
	case 'C':
		return 12;
	case 'd':
	case 'D':
		return 13;
	case 'e':
	case 'E':
		return 14;
	case 'f':
	case 'F':
		return 15;
	default:
		return buf - '0';
	}
}

void btmtk_usb_load_profile(struct hci_dev *hdev)
{
	mm_segment_t old_fs;
	struct file *file = NULL;
	unsigned char *buf;
	unsigned char target_buf[256 + 4] = { 0 };
	int i = 0;
	int j = 4;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	file = filp_open("/etc/Wireless/RT2870STA/BT_CONFIG.dat", O_RDONLY, 0);
	if (IS_ERR(file)) {
		set_fs(old_fs);
		return;
	}

	buf = kmalloc(1280, GFP_ATOMIC);
	if (!buf) {
		pr_err
		    ("malloc error when parsing /etc/Wireless/RT2870STA/BT_CONFIG.dat, exiting...\n");
		filp_close(file, NULL);
		set_fs(old_fs);
		return;
	}

	pr_warn("/etc/Wireless/RT2870STA/BT_CONFIG.dat exits, parse it.\n");
	memset(buf, 0, 1280);
	file->f_op->read(file, buf, 1280, &file->f_pos);

	for (i = 0; i < 1280; i++) {
		if (buf[i] == '\r')
			continue;
		if (buf[i] == '\n')
			continue;
		if (buf[i] == 0)
			break;
		if (buf[i] == '0' && buf[i + 1] == 'x') {
			i += 1;
			continue;
		}

		{
			if (buf[i + 1] == '\r' || buf[i + 1] == '\n' || buf[i + 1] == 0) {
				target_buf[j] = _ascii_to_int(buf[i]);
				j++;
			} else {
				target_buf[j] =
				    _ascii_to_int(buf[i]) << 4 | _ascii_to_int(buf[i + 1]);
				j++;
				i++;
			}
		}
	}
	kfree(buf);
	filp_close(file, NULL);
	set_fs(old_fs);

	/* Send to dongle */
	{
		target_buf[0] = 0xc3;
		target_buf[1] = 0xfc;
		target_buf[2] = j - 4 + 1;
		target_buf[3] = 0x01;

		pr_warn("Profile Configuration : (%d)\n", j);
		for (i = 0; i < j; i++)
			pr_warn("    0x%02X\n", target_buf[i]);

		if (hdev != NULL)
			btmtk_usb_submit_ctrl_urb(hdev, target_buf, j);
	}
}
#endif

static int btmtk_usb_open(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int err;

	pr_warn("%s\n", __func__);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return err;

	data->intf->needs_remote_wakeup = 1;

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_and_set_bit(BTUSB_INTR_RUNNING, &data->flags))
		goto done;

	err = btmtk_usb_submit_intr_urb(hdev, GFP_KERNEL);
	if (err < 0)
		goto failed;

	err = btmtk_usb_submit_bulk_in_urb(hdev, GFP_KERNEL);
	if (err < 0) {
		usb_kill_anchored_urbs(&data->intr_anchor);
		goto failed;
	}

	set_bit(BTUSB_BULK_RUNNING, &data->flags);
	/* btmtk_usb_submit_bulk_in_urb(hdev, GFP_KERNEL); */


#if LOAD_PROFILE
	btmtk_usb_load_profile(hdev);
#endif

#if SUPPORT_FW_DUMP
	{
		/* sema_init(&data->fw_dump_semaphore, 0); */
		init_completion(&data->fw_completion);
		data->fw_dump_tsk =
		    kthread_create(btmtk_usb_fw_dump_thread, (void *)data,
				   "btmtk_usb_fw_dump_thread");
		if (IS_ERR(data->fw_dump_tsk)) {
			pr_err("%s : create fw dump thread failed!\n", __func__);
			err = PTR_ERR(data->fw_dump_tsk);
			data->fw_dump_tsk = NULL;
			goto failed;
		}
		fw_dump_task_should_stop = 0;
		fw_dump_ptr = kmalloc(FW_DUMP_BUF_SIZE, GFP_ATOMIC);
		if (fw_dump_ptr == NULL) {
			pr_err("%s : kmalloc(%d) failed!\n", __func__, FW_DUMP_BUF_SIZE);
			goto failed;
		}
		fw_dump_file = NULL;
		fw_dump_read_ptr = fw_dump_ptr;
		fw_dump_write_ptr = fw_dump_ptr;
		fw_dump_total_read_size = 0;
		fw_dump_total_write_size = 0;
		fw_dump_buffer_used_size = 0;
		fw_dump_buffer_full = 0;
		fw_dump_bulk_urb = NULL;
		data->fw_dump_end_check_tsk = NULL;
		wake_up_process(data->fw_dump_tsk);
	}
#endif

 done:
	usb_autopm_put_interface(data->intf);
	return 0;

 failed:
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);
	clear_bit(HCI_RUNNING, &hdev->flags);
	usb_autopm_put_interface(data->intf);
	return err;
}

static void btmtk_usb_stop_traffic(struct btmtk_usb_data *data)
{
	pr_warn("%s", __func__);

	usb_kill_anchored_urbs(&data->intr_anchor);
	usb_kill_anchored_urbs(&data->bulk_anchor);
	/* SCO */
	/* usb_kill_anchored_urbs(&data->isoc_anchor); */
}

static int btmtk_usb_close(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int err;

	pr_warn("%s\n", __func__);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

#if SUPPORT_FW_DUMP
	if (data->fw_dump_tsk) {
		fw_dump_task_should_stop = 1;
		/*up(&data->fw_dump_semaphore); */
		complete(&data->fw_completion);
		kthread_stop(data->fw_dump_tsk);
		data->fw_dump_tsk = NULL;
		kfree(fw_dump_ptr);
	}
	if (data->fw_dump_end_check_tsk)
		fw_dump_end_checking_task_should_stop = 1;
#endif

	cancel_work_sync(&data->work);
	cancel_work_sync(&data->waker);
	/* SCO */
	/* clear_bit(BTUSB_ISOC_RUNNING, &data->flags); */
	clear_bit(BTUSB_BULK_RUNNING, &data->flags);
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);

	btmtk_usb_stop_traffic(data);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		goto failed;

	data->intf->needs_remote_wakeup = 0;
	usb_autopm_put_interface(data->intf);

 failed:
	usb_scuttle_anchored_urbs(&data->deferred);
	return 0;
}

static int btmtk_usb_flush(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	pr_info("%s", __func__);
	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void btmtk_usb_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	pr_info("%s: %s urb %p status %d count %d\n", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

 done:
	spin_lock(&data->txlock);
	data->tx_in_flight--;
	spin_unlock(&data->txlock);

	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static void btmtk_usb_isoc_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;

	pr_info("%s: %s urb %p status %d count %d", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

 done:
	kfree(urb->setup_packet);

	kfree_skb(skb);
}


static int btmtk_usb_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
/* #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0) */
/*	struct hci_dev *hdev = (struct hci_dev *)skb->dev; */
/*#endif */

	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	unsigned int pipe;
	int err;

	pr_info("%s\n", __func__);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	skb->dev = (void *)hdev;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
#if SUPPORT_UNPREDICTED_HCI_CMD_FILTER
		if (skb->len == 6 && skb->data[0] == 0xc9 &&
		    skb->data[1] == 0xfc &&
		    skb->data[2] == 0x03 &&
		    skb->data[3] == 0x01 && skb->data[4] == 0x0a && skb->data[5] == 0x00) {
			pr_warn
			    ("btmtk_usb : unpredicetd HCI command : %02X %02X %02X %02X %02X %02X\n",
			     skb->data[0], skb->data[1], skb->data[2], skb->data[3], skb->data[4],
			     skb->data[5]);
			return 0;
		}
#endif

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		dr = kmalloc(sizeof(*dr), GFP_ATOMIC);
		if (!dr) {
			usb_free_urb(urb);
			return -ENOMEM;
		}

		dr->bRequestType = data->cmdreq_type;
		dr->bRequest = 0;
		dr->wIndex = 0;
		dr->wValue = 0;
		dr->wLength = __cpu_to_le16(skb->len);

		pipe = usb_sndctrlpipe(data->udev, 0x00);

		if (test_bit(HCI_RUNNING, &hdev->flags)) {
			u16 op_code;

			memcpy(&op_code, skb->data, 2);
			pr_info("ogf = %x\n", (op_code & 0xfc00) >> 10);
			pr_info("ocf = %x\n", op_code & 0x03ff);
		}

		usb_fill_control_urb(urb, data->udev, pipe, (void *)dr,
				     skb->data, skb->len, btmtk_usb_tx_complete, skb);

		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		if (!data->bulk_tx_ep)
			return -ENODEV;

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndbulkpipe(data->udev, data->bulk_tx_ep->bEndpointAddress);

		usb_fill_bulk_urb(urb, data->udev, pipe,
				  skb->data, skb->len, btmtk_usb_tx_complete, skb);

		hdev->stat.acl_tx++;
		pr_info("HCI_ACLDATA_PKT:\n");
		break;

	case HCI_SCODATA_PKT:
		if (!data->isoc_tx_ep || hdev->conn_hash.sco_num < 1)
			return -ENODEV;

		urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndisocpipe(data->udev, data->isoc_tx_ep->bEndpointAddress);

		usb_fill_int_urb(urb, data->udev, pipe,
				 skb->data, skb->len, btmtk_usb_isoc_tx_complete,
				 skb, data->isoc_tx_ep->bInterval);

		urb->transfer_flags = URB_ISO_ASAP;

		__fill_isoc_descriptor(urb, skb->len,
				       le16_to_cpu(data->isoc_tx_ep->wMaxPacketSize));

		hdev->stat.sco_tx++;
		pr_info("HCI_SCODATA_PKT:\n");
		goto skip_waking;

	default:
		return -EILSEQ;
	}

	err = inc_tx(data);

	if (err) {
		usb_anchor_urb(urb, &data->deferred);
		schedule_work(&data->waker);
		err = 0;
		goto done;
	}

 skip_waking:
	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

 done:
	usb_free_urb(urb);
	return err;
}

static void btmtk_usb_notify(struct hci_dev *hdev, unsigned int evt)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	pr_warn("%s evt %d", hdev->name, evt);

	if (hdev->conn_hash.sco_num != data->sco_num) {
		data->sco_num = hdev->conn_hash.sco_num;
		schedule_work(&data->work);
	}
}

static inline int __set_isoc_interface(struct hci_dev *hdev, int altsetting)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct usb_interface *intf = data->isoc;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;

	if (!data->isoc)
		return -ENODEV;

	err = usb_set_interface(data->udev, 1, altsetting);
	if (err < 0) {
		pr_err("%s setting interface failed (%d)", hdev->name, -err);
		return err;
	}

	data->isoc_altsetting = altsetting;

	data->isoc_tx_ep = NULL;
	data->isoc_rx_ep = NULL;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->isoc_tx_ep && usb_endpoint_is_isoc_out(ep_desc)) {
			data->isoc_tx_ep = ep_desc;
			continue;
		}

		if (!data->isoc_rx_ep && usb_endpoint_is_isoc_in(ep_desc)) {
			data->isoc_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->isoc_tx_ep || !data->isoc_rx_ep) {
		pr_warn("%s invalid SCO descriptors", hdev->name);
		return -ENODEV;
	}

	return 0;
}

static void btmtk_usb_work(struct work_struct *work)
{
	struct btmtk_usb_data *data = container_of(work, struct btmtk_usb_data, work);
	struct hci_dev *hdev = data->hdev;
	int new_alts;
	int err;

	pr_warn("%s\n", __func__);

	if (hdev->conn_hash.sco_num > 0) {
		if (!test_bit(BTUSB_DID_ISO_RESUME, &data->flags)) {
			err = usb_autopm_get_interface(data->isoc ? data->isoc : data->intf);
			if (err < 0) {
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
				usb_kill_anchored_urbs(&data->isoc_anchor);
				return;
			}

			set_bit(BTUSB_DID_ISO_RESUME, &data->flags);
		}

		if (hdev->voice_setting & 0x0020) {
			static const int alts[3] = { 2, 4, 5 };

			new_alts = alts[hdev->conn_hash.sco_num - 1];
		} else if (hdev->voice_setting & 0x2000) {
			new_alts = 4;
		} else if (hdev->voice_setting & 0x0200) {
			new_alts = 6;	/* Alt setting for WBS SCO support */
		} else {
			new_alts = hdev->conn_hash.sco_num;
		}

		if (data->isoc_altsetting != new_alts) {
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			usb_kill_anchored_urbs(&data->isoc_anchor);

			if (__set_isoc_interface(hdev, new_alts) < 0)
				return;
		}

		if (!test_and_set_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
			if (btmtk_usb_submit_isoc_in_urb(hdev, GFP_KERNEL) < 0)
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			else
				btmtk_usb_submit_isoc_in_urb(hdev, GFP_KERNEL);
		}
	} else {
		clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		usb_kill_anchored_urbs(&data->isoc_anchor);

		__set_isoc_interface(hdev, 0);

		if (test_and_clear_bit(BTUSB_DID_ISO_RESUME, &data->flags))
			usb_autopm_put_interface(data->isoc ? data->isoc : data->intf);
	}
}

static void btmtk_usb_waker(struct work_struct *work)
{
	struct btmtk_usb_data *data = container_of(work, struct btmtk_usb_data, waker);
	int err;

	err = usb_autopm_get_interface(data->intf);

	if (err < 0)
		return;

	usb_autopm_put_interface(data->intf);
}

static int btmtk_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct btmtk_usb_data *data;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;
	struct hci_dev *hdev;

	pr_warn("===========================================\n");
	pr_warn("Mediatek Bluetooth USB driver ver %s\n", VERSION);
	pr_warn("===========================================\n");
	memset(driver_version, 0, sizeof(driver_version));
	memcpy(driver_version, VERSION, sizeof(VERSION));
	(void)fw_version;
	probe_counter++;

	pr_warn("btmtk_usb_probe begin!!\n");
	pr_warn("btmtk_usb counter = %d\n", probe_counter);

	/* interface numbers are hardcoded in the spec */
	if (intf->cur_altsetting->desc.bInterfaceNumber != 0) {
		pr_err("[ERR] interface number != 0 (%d)\n",
		       intf->cur_altsetting->desc.bInterfaceNumber);
		pr_err("btmtk_usb_probe end Error 1\n");
		btmtk_usb_toggle_rst_pin();
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);

	if (!data) {
		pr_err("[ERR] kzalloc failed !\n");
		pr_err("btmtk_usb_probe end Error 2\n");
		btmtk_usb_toggle_rst_pin();
		return -ENOMEM;
	}

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->intr_ep && usb_endpoint_is_int_in(ep_desc)) {
			data->intr_ep = ep_desc;
			continue;
		}

		if (!data->bulk_tx_ep && usb_endpoint_is_bulk_out(ep_desc)) {
			data->bulk_tx_ep = ep_desc;
			continue;
		}

		if (!data->bulk_rx_ep && usb_endpoint_is_bulk_in(ep_desc)) {
			data->bulk_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->intr_ep || !data->bulk_tx_ep || !data->bulk_rx_ep) {
		kfree(data);
		pr_err("btmtk_usb_probe end Error 3\n");
		btmtk_usb_toggle_rst_pin();
		return -ENODEV;
	}

	data->cmdreq_type = USB_TYPE_CLASS;

	data->udev = interface_to_usbdev(intf);
	data->intf = intf;

	spin_lock_init(&data->lock);
	INIT_WORK(&data->work, btmtk_usb_work);
	INIT_WORK(&data->waker, btmtk_usb_waker);
	spin_lock_init(&data->txlock);

	init_usb_anchor(&data->tx_anchor);
	init_usb_anchor(&data->intr_anchor);
	init_usb_anchor(&data->bulk_anchor);
	init_usb_anchor(&data->isoc_anchor);
	init_usb_anchor(&data->deferred);

	data->io_buf = kmalloc(256, GFP_ATOMIC);

	btmtk_usb_switch_iobase(data, WLAN);

	btmtk_usb_cap_init(data);

	if (data->need_load_rom_patch) {
		err = btmtk_usb_load_rom_patch(data);

		if (err < 0) {
			kfree(data->io_buf);
			kfree(data);
			pr_err("btmtk_usb_probe end Error 4\n");
			btmtk_usb_toggle_rst_pin();
			return err;
		}
	}

	if (data->need_load_fw) {
		err = btmtk_usb_load_fw(data);

		if (err < 0) {
			kfree(data->io_buf);
			kfree(data);
			pr_err("btmtk_usb_probe end Error 5\n");
			btmtk_usb_toggle_rst_pin();
			return err;
		}
	}

	hdev = hci_alloc_dev();
	if (!hdev) {
		kfree(data);
		pr_err("btmtk_usb_probe end Error 6\n");
		btmtk_usb_toggle_rst_pin();
		return -ENOMEM;
	}

	hdev->bus = HCI_USB;
	hci_set_drvdata(hdev, data);

	data->hdev = hdev;

	SET_HCIDEV_DEV(hdev, &intf->dev);

	hdev->open = btmtk_usb_open;
	hdev->close = btmtk_usb_close;
	hdev->flush = btmtk_usb_flush;
	hdev->send = btmtk_usb_send_frame;
	hdev->notify = btmtk_usb_notify;

	/* Interface numbers are hardcoded in the specification */
	data->isoc = usb_ifnum_to_if(data->udev, 1);

	kfree(data->rom_patch_bin_file_name);

	if (data->isoc) {
		err = usb_driver_claim_interface(&btmtk_usb_driver, data->isoc, data);
		if (err < 0) {
			hci_free_dev(hdev);
			kfree(data->io_buf);
			kfree(data);
			pr_err("btmtk_usb_probe end Error 7\n");
			btmtk_usb_toggle_rst_pin();
			return err;
		}
	}

	err = hci_register_dev(hdev);
	if (err < 0) {
		hci_free_dev(hdev);
		kfree(data->io_buf);
		kfree(data);
		pr_err("btmtk_usb_probe end Error 8\n");
		btmtk_usb_toggle_rst_pin();
		return err;
	}

	usb_set_intfdata(intf, data);
	pr_warn("btmtk_usb_probe end\n");
	return 0;
}

static void btmtk_usb_disconnect(struct usb_interface *intf)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev;

	pr_warn("%s\n", __func__);

	if (!data)
		return;

	hdev = data->hdev;

	usb_set_intfdata(data->intf, NULL);

	if (data->isoc)
		usb_set_intfdata(data->isoc, NULL);

	hci_unregister_dev(hdev);

	if (intf == data->isoc)
		usb_driver_release_interface(&btmtk_usb_driver, data->intf);
	else if (data->isoc)
		usb_driver_release_interface(&btmtk_usb_driver, data->isoc);

	hci_free_dev(hdev);
	kfree(data->io_buf);

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		if (data->need_load_rom_patch)
			kfree(data->rom_patch);

		if (data->need_load_fw)
			kfree(data->fw_image);
	}

	kfree(data);
}

#ifdef CONFIG_PM
static int btmtk_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);

	pr_warn("btmtk_usb_suspend\n");

	if (data->suspend_count++)
		return 0;

#if BT_SEND_HCI_CMD_BEFORE_SUSPEND
	btmtk_usb_send_hci_suspend_cmd(data->udev);
#endif

	spin_lock_irq(&data->txlock);
	if (!(PMSG_IS_AUTO(message) && data->tx_in_flight)) {
		set_bit(BTUSB_SUSPENDING, &data->flags);
		spin_unlock_irq(&data->txlock);
	} else {
		spin_unlock_irq(&data->txlock);
		data->suspend_count--;
		return -EBUSY;
	}

	cancel_work_sync(&data->work);

	btmtk_usb_stop_traffic(data);
	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void play_deferred(struct btmtk_usb_data *data)
{
	struct urb *urb;
	int err;

	while ((urb = usb_get_from_anchor(&data->deferred))) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err < 0)
			break;

		data->tx_in_flight++;
	}

	usb_scuttle_anchored_urbs(&data->deferred);
}

static int btmtk_usb_resume(struct usb_interface *intf)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev = data->hdev;
	int err = 0;

	pr_warn("%s\n", __func__);

	if (--data->suspend_count)
		return 0;

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_bit(BTUSB_INTR_RUNNING, &data->flags)) {
		err = btmtk_usb_submit_intr_urb(hdev, GFP_NOIO);
		if (err < 0) {
			pr_warn("resume interrrupt urb fail, error:%d", err);
			clear_bit(BTUSB_INTR_RUNNING, &data->flags);
			goto failed;
		}
	}

	if (test_bit(BTUSB_BULK_RUNNING, &data->flags)) {
		err = btmtk_usb_submit_bulk_in_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_BULK_RUNNING, &data->flags);
			pr_warn("resume bulk urb fail, error:%d", err);
			goto failed;
		}

		btmtk_usb_submit_bulk_in_urb(hdev, GFP_NOIO);
	}

	if (test_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
		if (btmtk_usb_submit_isoc_in_urb(hdev, GFP_NOIO) < 0) {
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			pr_warn("resume isoc urb fail, error:%d", err);
		} else
			btmtk_usb_submit_isoc_in_urb(hdev, GFP_NOIO);
	}

	spin_lock_irq(&data->txlock);
	play_deferred(data);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);
	schedule_work(&data->work);

	return 0;

 failed:
	usb_scuttle_anchored_urbs(&data->deferred);
 done:
	spin_lock_irq(&data->txlock);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);

	return err;
}
#endif

static struct usb_device_id btmtk_usb_table[] = {
	{USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7662, 0xe0, 0x01, 0x01), .bInterfaceNumber = 0},
	{USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7632, 0xe0, 0x01, 0x01), .bInterfaceNumber = 0},
	{}
};

static struct usb_driver btmtk_usb_driver = {
	.name = "btmtk_usb",
	.probe = btmtk_usb_probe,
	.disconnect = btmtk_usb_disconnect,
#ifdef CONFIG_PM
	.suspend = btmtk_usb_suspend,
	.resume = btmtk_usb_resume,
#endif
	.id_table = btmtk_usb_table,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(btmtk_usb_driver);


MODULE_DESCRIPTION("Mediatek Bluetooth USB driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
MODULE_FIRMWARE("mt7662_patch_e1_hdr.bin");
MODULE_FIRMWARE("mt7662_patch_e3_hdr.bin");
