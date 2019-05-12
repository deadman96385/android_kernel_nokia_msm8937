
/*s program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include "msm_flash.h"
#include "msm_camera_dt_util.h"
#include "msm_cci.h"
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/fs.h>




#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)


#define LM36010_NAME "leds-lm36010"
DEFINE_MSM_MUTEX(msm_flash_mutex);




static struct v4l2_file_operations msm_flash_v4l2_subdev_fops;
struct i2c_client *lm36010_i2c_client;

#define USE_GPIO 0


#define	REG_FILT_TIME			(0x0)
#define	REG_IVFM_MODE			(0x2)
#define	REG_TORCH_TIME			(0x0)
#define	REG_FLASH_TIME			(0x2)
#define	REG_I_CTRL_F			(0x3)
#define	REG_I_CTRL_T			(0x4)
#define	REG_ENABLE			(0x1)
#define	REG_FLAG			(0x6)
#define	REG_MAX				(0x6)

#define	UVLO_EN_SHIFT			(7)
#define	IVM_D_TH_SHIFT			(2)
#define	TORCH_RAMP_UP_TIME_SHIFT	(3)
#define	TORCH_RAMP_DN_TIME_SHIFT	(0)
#define	INDUCTOR_I_LIMIT_SHIFT		(6)
#define	FLASH_RAMP_TIME_SHIFT		(3)
#define	FLASH_TOUT_TIME_SHIFT		(1)
#define	TORCH_I_SHIFT			(0)
#define	FLASH_I_SHIFT			(0)
#define	IVFM_SHIFT			(7)
#define	TX_PIN_EN_SHIFT			(6)
#define	STROBE_PIN_EN_SHIFT		(5)
#define	TORCH_PIN_EN_SHIFT		(4)
#define	MODE_BITS_SHIFT			(0)

#define	UVLO_EN_MASK			(0x1)
#define	IVM_D_TH_MASK			(0x7)
#define	TORCH_RAMP_UP_TIME_MASK		(0x7)
#define	TORCH_RAMP_DN_TIME_MASK		(0x7)
#define	INDUCTOR_I_LIMIT_MASK		(0x1)
#define	FLASH_RAMP_TIME_MASK		(0x7)
#define	FLASH_TOUT_TIME_MASK		(0x1E)
#define	TORCH_I_MASK			(0x7F)
#define	FLASH_I_MASK			(0x7F)
#define	IVFM_MASK			(0x1)
#define	TX_PIN_EN_MASK			(0x1)
#define	STROBE_PIN_EN_MASK		(0x1)
#define	TORCH_PIN_EN_MASK		(0x1)
//#define	MODE_BITS_MASK			(0xFF)
#define	MODE_BITS_MASK			(0x03)
#define EX_PIN_CONTROL_MASK		(0x71)
#define EX_PIN_ENABLE_MASK		(0x70)
#define CURRENT_FLASH_LIMIT_VALUE		(0x9)
#define CURRENT_TORCH_LIMIT_VALUE		(0x44)
#define CURRENT_MAX_VALUE		(0xff)
#define TORCH_CURRENT_MAX_VALUE		(375)
#define TORCH_HAL_CURENT_VALUE (200)

enum lm36010_mode {
	MODES_STASNDBY = 0,
	MODES_INDIC,
	MODES_TORCH,
	MODES_FLASH
};

struct lm36010_config_data {
	struct led_classdev cdev_torch;
	struct led_classdev cdev_switch;
	u8 torch_value;
	u8 switch_value;
	struct work_struct work_torch;
	struct work_struct work_switch;
	struct regmap *regmap;
	struct mutex lock;
	unsigned int last_flag;
};

struct lm36010_config_data g_lm36010_data;


static int lm36010_status_control(struct lm36010_config_data *chip,
			  u8 brightness, enum lm36010_mode opmode)
{
	int ret;
	static u8 torch_not_work_flags=0;
	u8 old_brightness=0;
	
	ret = regmap_read(chip->regmap, REG_FLAG, &chip->last_flag);
	if (ret < 0) {
		pr_err("Failed to read REG_FLAG Register\n");
		goto out;
	}

	if (chip->last_flag)
		pr_err("Last FLAG is 0x%x\n", chip->last_flag);


	/* brightness 0 means off-state */
	old_brightness=brightness;
	if (!brightness)
		opmode = MODES_STASNDBY;

	switch (opmode) {
	case MODES_TORCH:
		CDBG("MODES_TORCH\n");
		if(old_brightness==1){
			CDBG("old_brightness==1\n");
			torch_not_work_flags=0;
			break;
		}
		if(brightness>0){
			brightness=(brightness*CURRENT_TORCH_LIMIT_VALUE)/CURRENT_MAX_VALUE;
			if(brightness==0)
		brightness=1;
		}
		CDBG("brightness:%d\n",brightness);
		ret = regmap_update_bits(chip->regmap, REG_I_CTRL_T,
					 TORCH_I_MASK << TORCH_I_SHIFT,
					 (brightness-1) << TORCH_I_SHIFT);
		CDBG("have set current value\n");
		if(old_brightness>1){
			torch_not_work_flags=1;
			CDBG("torch_not_work_flags=1\n");
		}
		break;

	case MODES_FLASH:
		if(brightness>0){
			brightness=(brightness*CURRENT_FLASH_LIMIT_VALUE)/CURRENT_MAX_VALUE;
		if(brightness==0)
			brightness=1;
		}
		ret = regmap_update_bits(chip->regmap, REG_I_CTRL_F,
					 FLASH_I_MASK << FLASH_I_SHIFT,
					 (brightness-1 ) << FLASH_I_SHIFT);

	case MODES_INDIC:
		ret = regmap_update_bits(chip->regmap, REG_I_CTRL_T,
					 TORCH_I_MASK << TORCH_I_SHIFT,
					 (brightness) << TORCH_I_SHIFT);
		break;

	case MODES_STASNDBY:
		torch_not_work_flags=0;
		break;

	default:
		return ret;
	}
	if (ret < 0) {
		CDBG("Failed to write REG_I_CTRL Register\n");
		goto out;
	}

	if(MODES_TORCH==opmode){
		if(1==torch_not_work_flags)
			return ret;
	}
	ret = regmap_update_bits(chip->regmap, REG_ENABLE,
				 MODE_BITS_MASK << MODE_BITS_SHIFT,
				 opmode << MODE_BITS_SHIFT);
out:
	return ret;
}


static void lm36010_torch_brightness_set(struct led_classdev *cdev,enum led_brightness brightness)
{
	struct lm36010_config_data *chip =container_of(cdev, struct lm36010_config_data, cdev_torch);

	mutex_lock(&chip->lock);
	chip->torch_value= (u8)brightness;
	schedule_work(&chip->work_torch);
	mutex_unlock(&chip->lock);
}

static void lm36010_torch_brightness_set_work(struct work_struct *work)
{
	struct lm36010_config_data *chip =
	    container_of(work, struct lm36010_config_data, work_torch);

	mutex_lock(&chip->lock);
	lm36010_status_control(chip, chip->torch_value, MODES_TORCH);
	mutex_unlock(&chip->lock);
}


static void lm36010_switch_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm36010_config_data *chip =
	    container_of(cdev, struct lm36010_config_data, cdev_switch);
	mutex_lock(&chip->lock);
	chip->switch_value= (u8)brightness;
	//lm36010_status_control(chip, chip->switch_value, MODES_TORCH);
	schedule_work(&chip->work_switch);
	mutex_unlock(&chip->lock);
}

static void lm36010_switch_brightness_set_work(struct work_struct *work)
{
	struct lm36010_config_data *chip =
	    container_of(work, struct lm36010_config_data, work_switch);

	mutex_lock(&chip->lock);
	lm36010_status_control(chip, chip->switch_value, MODES_TORCH);
	mutex_unlock(&chip->lock);
}
static const struct regmap_config lm36010_regmap = {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = REG_MAX,
};


static const struct of_device_id msm_flash_dt_match[] = {
	{.compatible = "qcom,leds-lm36010", .data = NULL},
	{}
};

static struct msm_flash_table msm_i2c_flash_table;
static struct msm_flash_table msm_gpio_flash_table;
static struct msm_flash_table msm_pmic_flash_table;

static struct msm_flash_table *flash_table[] = {
	&msm_i2c_flash_table,
	&msm_gpio_flash_table,
	&msm_pmic_flash_table
};

static int32_t msm_flash_get_subdev_id(
	struct msm_flash_ctrl_t *flash_ctrl, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		CDBG("failed\n");
		return -EINVAL;
	}
	if (flash_ctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = flash_ctrl->pdev->id;
	else
		*subdev_id = flash_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_i2c_write_table(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_camera_i2c_reg_setting_array *settings)
{
	int i,j;
    struct i2c_client* slave_client;
	struct lm36010_config_data * config_data;
	struct regmap* regmap;
	uint8_t data[settings->size*2];
	slave_client = flash_ctrl->flash_i2c_client.client;
	config_data=(struct lm36010_config_data *)i2c_get_clientdata(slave_client);
	if(config_data)
		regmap= config_data->regmap;
	
	CDBG("settings->size:%d\n", settings->size);	
	for(i=0,j=0;j<settings->size;){		
		data[i]= settings->reg_setting_a[j].reg_addr;	
		data[i+1]= settings->reg_setting_a[j].reg_data;		
		CDBG("data[%d]:0x%x,data[%d]:0x%x\n", i,data[i],i+1,data[i+1]);
		switch(data[i]){
			case REG_ENABLE:
				CDBG("REG_ENABLE\n");
				CDBG("data[%d]:0x%x,data[%d]:0x%x\n", i,data[i],i+1,data[i+1]);
				regmap_update_bits(regmap, REG_ENABLE, MODE_BITS_MASK << MODE_BITS_SHIFT,data[i+1]<<MODE_BITS_SHIFT);
				break;
			case REG_I_CTRL_T:
				CDBG("REG_I_CTRL_T\n");
				CDBG("data[%d]:0x%x,data[%d]:0x%x\n", i,data[i],i+1,data[i+1]);
				//regmap_update_bits(regmap,REG_I_CTRL_T, 0xff,data[i+1]);
				regmap_update_bits(regmap,REG_I_CTRL_T, 0x7f,data[i+1]);
				//regmap_update_bits(regmap, REG_I_CTRL_T,TORCH_I_MASK << TORCH_I_SHIFT,data[i+1] << TORCH_I_SHIFT);
				break;
			case REG_I_CTRL_F:
				CDBG("REG_I_CTRL_F\n");
				CDBG("data[%d]:0x%x,data[%d]:0x%x\n", i,data[i],i+1,data[i+1]);
				//regmap_update_bits(regmap,REG_I_CTRL_F, 0xff,data[i+1]);
				regmap_update_bits(regmap,REG_I_CTRL_F, 0x7f,data[i+1]);
				//regmap_update_bits(regmap, REG_I_CTRL_F,FLASH_I_MASK << FLASH_I_SHIFT,data[i+1] << FLASH_I_SHIFT);
				break;
			case REG_FLASH_TIME:
				CDBG("REG_FLASH_TIME\n");
				CDBG("data[%d]:0x%x,data[%d]:0x%x\n", i,data[i],i+1,data[i+1]);
				regmap_update_bits(regmap, REG_FLASH_TIME,FLASH_TOUT_TIME_MASK << FLASH_TOUT_TIME_SHIFT,data[i+1]<< FLASH_TOUT_TIME_SHIFT);
				break;
			case REG_TORCH_TIME:
				break;
			default:
				break;
		}
		i+=2;		
		j++;	
	}

	CDBG("ok");
	return 0;
}

/*
#ifdef CONFIG_COMPAT
static void msm_flash_copy_power_settings_compat(
	struct msm_sensor_power_setting *ps,
	struct msm_sensor_power_setting32 *ps32, uint32_t size)
{
	uint16_t i = 0;

	for (i = 0; i < size; i++) {
		ps[i].config_val = ps32[i].config_val;
		ps[i].delay = ps32[i].delay;
		ps[i].seq_type = ps32[i].seq_type;
		ps[i].seq_val = ps32[i].seq_val;
	}
}

#endif
*/
static int32_t msm_flash_i2c_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_flash_init_info_t *flash_init_info =
		flash_data->cfg.flash_init_info;
	struct msm_camera_i2c_reg_setting_array *settings = NULL;

	CDBG("Enter \r\n");
	
	//struct msm_camera_cci_client *cci_client = NULL;
#ifdef CONFIG_COMPAT
	//struct msm_sensor_power_setting_array32 *power_setting_array32 = NULL;
#endif

#if 0
	if (!flash_init_info || !flash_init_info->power_setting_array) {
		CDBG("%s:%d failed: Null pointer\n", __func__, __LINE__);
		return -EFAULT;
	}

#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		power_setting_array32 = kzalloc(
			sizeof(struct msm_sensor_power_setting_array32),
			GFP_KERNEL);
		if (!power_setting_array32) {
			CDBG("%s mem allocation failed %d\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		if (copy_from_user(power_setting_array32,
			(void *)flash_init_info->power_setting_array,
			sizeof(struct msm_sensor_power_setting_array32))) {
			CDBG("%s copy_from_user failed %d\n",
				__func__, __LINE__);
			kfree(power_setting_array32);
			return -EFAULT;
		}

		flash_ctrl->power_setting_array.size =
			power_setting_array32->size;
		flash_ctrl->power_setting_array.size_down =
			power_setting_array32->size_down;
		flash_ctrl->power_setting_array.power_down_setting =
			compat_ptr(power_setting_array32->power_down_setting);
		flash_ctrl->power_setting_array.power_setting =
			compat_ptr(power_setting_array32->power_setting);

		/* Validate power_up array size and power_down array size */
		if ((!flash_ctrl->power_setting_array.size) ||
			(flash_ctrl->power_setting_array.size >
			MAX_POWER_CONFIG) ||
			(!flash_ctrl->power_setting_array.size_down) ||
			(flash_ctrl->power_setting_array.size_down >
			MAX_POWER_CONFIG)) {

			CDBG("failed: invalid size %d, size_down %d",
				flash_ctrl->power_setting_array.size,
				flash_ctrl->power_setting_array.size_down);
			kfree(power_setting_array32);
			power_setting_array32 = NULL;
			return -EINVAL;
		}
		/* Copy the settings from compat struct to regular struct */
		msm_flash_copy_power_settings_compat(
			flash_ctrl->power_setting_array.power_setting_a,
			power_setting_array32->power_setting_a,
			flash_ctrl->power_setting_array.size);

		msm_flash_copy_power_settings_compat(
			flash_ctrl->power_setting_array.power_down_setting_a,
			power_setting_array32->power_down_setting_a,
			flash_ctrl->power_setting_array.size_down);
	} else
#endif
	if (copy_from_user(&flash_ctrl->power_setting_array,
		(void *)flash_init_info->power_setting_array,
		sizeof(struct msm_sensor_power_setting_array))) {
		CDBG("%s copy_from_user failed %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (flash_ctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = flash_ctrl->flash_i2c_client.cci_client;
		cci_client->sid = flash_init_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = flash_init_info->i2c_freq_mode;
	}

	flash_ctrl->power_info.power_setting =
		flash_ctrl->power_setting_array.power_setting_a;
	flash_ctrl->power_info.power_down_setting =
		flash_ctrl->power_setting_array.power_down_setting_a;
	flash_ctrl->power_info.power_setting_size =
		flash_ctrl->power_setting_array.size;
	flash_ctrl->power_info.power_down_setting_size =
		flash_ctrl->power_setting_array.size_down;

	rc = msm_camera_power_up(&flash_ctrl->power_info,
		flash_ctrl->flash_device_type,
		&flash_ctrl->flash_i2c_client);
	if (rc < 0) {
		CDBG("%s msm_camera_power_up failed %d\n",
			__func__, __LINE__);
		goto msm_flash_i2c_init_fail;
	}

#endif
//this is always init setttints
	if (flash_data->cfg.flash_init_info->settings) {
		settings = kzalloc(sizeof(
			struct msm_camera_i2c_reg_setting_array), GFP_KERNEL);
		if (!settings) {
			CDBG("%s mem allocation failed %d\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		if (copy_from_user(settings, (void *)flash_init_info->settings,
			sizeof(struct msm_camera_i2c_reg_setting_array))) {
			kfree(settings);
			CDBG("%s copy_from_user failed %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

	
		rc = msm_flash_i2c_write_table(flash_ctrl, settings);
		kfree(settings);

		if (rc < 0) {
			CDBG("%s:%d msm_flash_i2c_write_table rc %d failed\n",
				__func__, __LINE__, rc);
		}
	}

	return rc;

}

static int32_t msm_flash_gpio_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t i = 0;
	int32_t rc = 0;

	CDBG("Enter");
	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		flash_ctrl->flash_op_current[i] = LED_FULL;

	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		flash_ctrl->torch_op_current[i] = LED_HALF;

	for (i = 0; i < flash_ctrl->torch_num_sources; i++) {
		if (!flash_ctrl->torch_trigger[i]) {
			if (i < flash_ctrl->flash_num_sources)
				flash_ctrl->torch_trigger[i] =
					flash_ctrl->flash_trigger[i];
			else
				flash_ctrl->torch_trigger[i] =
					flash_ctrl->flash_trigger[
					flash_ctrl->flash_num_sources - 1];
		}
	}

	rc = flash_ctrl->func_tbl->camera_flash_off(flash_ctrl, flash_data);

	CDBG("Exit");
	return rc;
}

static int32_t msm_flash_i2c_off_current(
	struct msm_flash_ctrl_t *flash_ctrl)
{
  struct i2c_client* slave_client;
  struct lm36010_config_data * config_data;
  struct regmap* regmap;
  slave_client = flash_ctrl->flash_i2c_client.client;
  config_data=(struct lm36010_config_data *)i2c_get_clientdata(slave_client);
  if(config_data)
	  regmap= config_data->regmap;
  regmap_update_bits(regmap, REG_ENABLE, MODE_BITS_MASK << MODE_BITS_SHIFT,0);	
  return 0;
}
static int32_t msm_flash_i2c_release(
	struct msm_flash_ctrl_t *flash_ctrl)
{

  struct i2c_client* slave_client;
  struct lm36010_config_data * config_data;
  struct regmap* regmap;
  
  slave_client = flash_ctrl->flash_i2c_client.client;
  config_data=(struct lm36010_config_data *)i2c_get_clientdata(slave_client);
  if(config_data)
	  regmap= config_data->regmap;
  regmap_update_bits(regmap, REG_ENABLE, MODE_BITS_MASK << MODE_BITS_SHIFT,0);
			
  flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;		
  CDBG("EXIT \r\n");

#if 0
	struct i2c_client* slave_client;
	struct i2c_msg msg;
	 uint8_t data[2];
	if (!(&flash_ctrl->power_info) || !(&flash_ctrl->flash_i2c_client)) {
		CDBG("%s:%d failed: %pK %pK\n",
			__func__, __LINE__, &flash_ctrl->power_info,
			&flash_ctrl->flash_i2c_client);
		return -EINVAL;
	}
    data[0]=0x0a;
	data[1]=0x00;
	slave_client = flash_ctrl->flash_i2c_client.client;
	msg.addr=slave_client->addr;
	msg.len=2;
	msg.buf=(uint8_t *)data;

	if (i2c_transfer(slave_client->adapter, &msg, 1) < 0)
		return -EIO;
	#endif
	return 0;
}

static int32_t msm_flash_off(struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t i = 0;

	CDBG("Enter\n");

	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		if (flash_ctrl->flash_trigger[i])
			led_trigger_event(flash_ctrl->flash_trigger[i], 0);

	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		if (flash_ctrl->torch_trigger[i])
			led_trigger_event(flash_ctrl->torch_trigger[i], 0);
	if (flash_ctrl->switch_trigger)
		led_trigger_event(flash_ctrl->switch_trigger, 0);

	CDBG("Exit\n");
	return 0;
}


static int32_t msm_flash_high_i2c_write_setting_array(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting_array *settings = NULL;

	if (!flash_data->cfg.settings) {
		CDBG("%s:%d failed: Null pointer\n", __func__, __LINE__);
		return -EFAULT;
	}

	settings = kzalloc(sizeof(struct msm_camera_i2c_reg_setting_array),
		GFP_KERNEL);
	if (!settings) {
		CDBG("%s mem allocation failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(settings, (void *)flash_data->cfg.settings,
		sizeof(struct msm_camera_i2c_reg_setting_array))) {
		kfree(settings);
		CDBG("%s copy_from_user failed %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	#if USE_GPIO
	gpio_set_value_cansleep(
			flash_ctrl->power_info.gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN],
			GPIO_OUT_HIGH);
	#endif
	rc = msm_flash_i2c_write_table(flash_ctrl, settings);
	kfree(settings);

	if (rc < 0) {
		CDBG("%s:%d msm_flash_i2c_write_table rc = %d failed\n",
			__func__, __LINE__, rc);
	}
	return rc;
}

static int32_t msm_flash_low_i2c_write_setting_array(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting_array *settings = NULL;
	struct lm36010_config_data *chip=NULL;
	int32_t count,i,brightness;

	CDBG("Enter \r\n");
	if (!flash_data->cfg.settings) {
		//When use torch icon in android, HAL transfort data is flash_current not settings
		//if we not deal with the flash_current,this function will return -EFAULT,Torch can't work
		//So I add these code.Normal mode pre flash will use settings data
		count = ARRAY_SIZE(flash_data->flash_current);
		if(count > MAX_LED_TRIGGERS){
			CDBG("%s:%d :count:%d > 3\n", __func__, __LINE__,count);
			return -EFAULT;

		}
		CDBG("%s:%d :count:%d\n", __func__, __LINE__,count);
		for(i=0;i< count;i++){
		//maybe hal not set seeting,and it will set torch_current
			if(flash_data->flash_current[i]!=0 ){
				if(brightness>TORCH_CURRENT_MAX_VALUE)
					brightness=TORCH_CURRENT_MAX_VALUE;
				
				brightness=flash_data->flash_current[i];
				if(brightness>TORCH_CURRENT_MAX_VALUE)
					brightness=TORCH_CURRENT_MAX_VALUE;
				CDBG("%s:%d flash_current:%d\n", __func__, __LINE__,brightness);
				brightness=(brightness*CURRENT_TORCH_LIMIT_VALUE)/TORCH_CURRENT_MAX_VALUE;
				if(brightness==0)
					brightness=1;
				CDBG("%s:%d brightness:%d\n", __func__, __LINE__,brightness);
				chip=(struct lm36010_config_data*)i2c_get_clientdata(flash_ctrl->flash_i2c_client.client);
				if(chip && chip->regmap){
					rc = regmap_update_bits(chip->regmap, REG_I_CTRL_T,
							 TORCH_I_MASK << TORCH_I_SHIFT,
							 (brightness-1) << TORCH_I_SHIFT);
					rc = regmap_update_bits(chip->regmap, REG_ENABLE,
						 MODE_BITS_MASK << MODE_BITS_SHIFT,
						 MODES_TORCH << MODE_BITS_SHIFT);
					//return 0;
				}
				else{
					CDBG("%s:%d flm36010_config_data in NULL\n", __func__, __LINE__);
					return -EFAULT;
				}
			}
		}
		return 0;
	}

	settings = kzalloc(sizeof(struct msm_camera_i2c_reg_setting_array),
		GFP_KERNEL);
	if (!settings) {
		CDBG("%s mem allocation failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(settings, (void *)flash_data->cfg.settings,
		sizeof(struct msm_camera_i2c_reg_setting_array))) {
		kfree(settings);
		CDBG("%s copy_from_user failed %d\n", __func__, __LINE__);
		return -EFAULT;
	}
	
	#if USE_GPIO
	gpio_set_value_cansleep(
		flash_ctrl->power_info.gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);
	#endif
	rc = msm_flash_i2c_write_table(flash_ctrl, settings);
	kfree(settings);

	if (rc < 0) {
		CDBG("%s:%d msm_flash_i2c_write_table rc = %d failed\n",
			__func__, __LINE__, rc);
	}
	return rc;
}

static int32_t msm_flash_off_i2c_write_setting_array(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting_array *settings = NULL;

	if (!flash_data->cfg.settings) {
		//When use torch icon in android, HAL transfort data is flash_current not settings
		//if we not deal with the flash_current,this function will return -EFAULT,Torch can't work
		//So I add these code.Normal mode pre flash will use settings data
		msm_flash_i2c_off_current(flash_ctrl);
		return 0;
	}

	settings = kzalloc(sizeof(struct msm_camera_i2c_reg_setting_array),
		GFP_KERNEL);
	if (!settings) {
		CDBG("%s mem allocation failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(settings, (void *)flash_data->cfg.settings,
		sizeof(struct msm_camera_i2c_reg_setting_array))) {
		kfree(settings);
		CDBG("%s copy_from_user failed %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	rc = msm_flash_i2c_write_table(flash_ctrl, settings);
	kfree(settings);

	if (rc < 0) {
		CDBG("%s:%d msm_flash_i2c_write_table rc = %d failed\n",
			__func__, __LINE__, rc);
	}

	#if USE_GPIO
	gpio_set_value_cansleep(
		flash_ctrl->power_info.gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
	gpio_set_value_cansleep(
			flash_ctrl->power_info.gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN],
			GPIO_OUT_LOW);
	#endif
	return rc;
}

static int32_t msm_flash_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	uint32_t i = 0;
	int32_t rc = -EFAULT;
	static enum msm_flash_driver_type first_time_init_flash_type=0xff;
	enum msm_flash_driver_type flash_driver_type = FLASH_DRIVER_DEFAULT;

	CDBG("Enter");

	if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT) {
		CDBG("%s:%d Invalid flash state = %d",
			__func__, __LINE__, flash_ctrl->flash_state);
		return 0;
	}

	//hal init the flash again ,will use FLASH_DRIVER_DEFAULT,can't find the right driver type
	if(first_time_init_flash_type !=0xff)
		flash_data->cfg.flash_init_info->flash_driver_type = first_time_init_flash_type;
	CDBG("flash_ctrl->flash_driver_type=%d",flash_ctrl->flash_driver_type);
	CDBG("flash_data->cfg.flash_init_info->flash_driver_type=%d",flash_data->cfg.flash_init_info->flash_driver_type);
	if (flash_data->cfg.flash_init_info->flash_driver_type ==
		FLASH_DRIVER_DEFAULT) {
		flash_driver_type = flash_ctrl->flash_driver_type;
		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			flash_data->flash_current[i] =
				flash_ctrl->flash_max_current[i];
			flash_data->flash_duration[i] =
				flash_ctrl->flash_max_duration[i];
		}
	} else if (flash_data->cfg.flash_init_info->flash_driver_type ==
		flash_ctrl->flash_driver_type) {
		flash_driver_type = flash_ctrl->flash_driver_type;
		for (i = 0; i < MAX_LED_TRIGGERS; i++) {
			flash_ctrl->flash_max_current[i] =
				flash_data->flash_current[i];
			flash_ctrl->flash_max_duration[i] =
					flash_data->flash_duration[i];
		}
	}

	if (flash_driver_type == FLASH_DRIVER_DEFAULT) {
		CDBG("%s:%d invalid flash_driver_type", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(flash_table); i++) {
		if (flash_driver_type == flash_table[i]->flash_driver_type) {
			flash_ctrl->func_tbl = &flash_table[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		CDBG("%s:%d failed invalid flash_driver_type %d\n",
			__func__, __LINE__,
			flash_data->cfg.flash_init_info->flash_driver_type);
	}

	if (flash_ctrl->func_tbl->camera_flash_init) {
		rc = flash_ctrl->func_tbl->camera_flash_init(
				flash_ctrl, flash_data);
		if (rc < 0) {
			CDBG("%s:%d camera_flash_init failed rc = %d",
				__func__, __LINE__, rc);
			return rc;
		}
	}

	flash_ctrl->flash_state = MSM_CAMERA_FLASH_INIT;
	first_time_init_flash_type = flash_driver_type;

	CDBG("Exit");
	return 0;
}

static int32_t msm_flash_low(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	uint32_t curr = 0, max_current = 0;
	int32_t i = 0;

	CDBG("Enter\n");
	/* Turn off flash triggers */
	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		if (flash_ctrl->flash_trigger[i])
			led_trigger_event(flash_ctrl->flash_trigger[i], 0);

	/* Turn on flash triggers */
	for (i = 0; i < flash_ctrl->torch_num_sources; i++) {
		if (flash_ctrl->torch_trigger[i]) {
			max_current = flash_ctrl->torch_max_current[i];
			if (flash_data->flash_current[i] >= 0 &&
				flash_data->flash_current[i] <
				max_current) {
				curr = flash_data->flash_current[i];
			} else {
				curr = flash_ctrl->torch_op_current[i];
				pr_debug("LED current clamped to %d\n",
					curr);
			}
			CDBG("low_flash_current[%d] = %d", i, curr);
			led_trigger_event(flash_ctrl->torch_trigger[i],
				curr);
		}
	}
	if (flash_ctrl->switch_trigger)
		led_trigger_event(flash_ctrl->switch_trigger, 1);
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_high(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t curr = 0;
	int32_t max_current = 0;
	int32_t i = 0;

	/* Turn off torch triggers */
	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		if (flash_ctrl->torch_trigger[i])
			led_trigger_event(flash_ctrl->torch_trigger[i], 0);

	/* Turn on flash triggers */
	for (i = 0; i < flash_ctrl->flash_num_sources; i++) {
		if (flash_ctrl->flash_trigger[i]) {
			max_current = flash_ctrl->flash_max_current[i];
			if (flash_data->flash_current[i] >= 0 &&
				flash_data->flash_current[i] <
				max_current) {
				curr = flash_data->flash_current[i];
			} else {
				curr = flash_ctrl->flash_op_current[i];
				pr_debug("LED flash_current[%d] clamped %d\n",
					i, curr);
			}
			CDBG("high_flash_current[%d] = %d", i, curr);
			led_trigger_event(flash_ctrl->flash_trigger[i],
				curr);
		}
	}
	if (flash_ctrl->switch_trigger)
		led_trigger_event(flash_ctrl->switch_trigger, 1);
	return 0;
}

static int32_t msm_flash_release(
	struct msm_flash_ctrl_t *flash_ctrl)
{
	int32_t rc = 0;
	if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_RELEASE) {
		CDBG("%s:%d Invalid flash state = %d",
			__func__, __LINE__, flash_ctrl->flash_state);
		return 0;
	}

	rc = flash_ctrl->func_tbl->camera_flash_off(flash_ctrl, NULL);
	if (rc < 0) {
		CDBG("%s:%d camera_flash_init failed rc = %d",
			__func__, __LINE__, rc);
		return rc;
	}
	 CDBG("flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE");
	flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;
	return 0;
}

static int32_t msm_flash_init_prepare(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	struct msm_flash_cfg_data_t flash_data_k;
	struct msm_flash_init_info_t flash_init_info;
	int32_t i = 0;
	flash_data_k.cfg_type = flash_data->cfg_type;
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		flash_data_k.flash_current[i] =
			flash_data->flash_current[i];
		flash_data_k.flash_duration[i] =
			flash_data->flash_duration[i];
	}

	flash_data_k.cfg.flash_init_info = &flash_init_info;
	if (copy_from_user(&flash_init_info,
		(void __user *)(flash_data->cfg.flash_init_info),
		sizeof(struct msm_flash_init_info_t))) {
		pr_err("%s copy_from_user failed %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	return msm_flash_init(flash_ctrl, &flash_data_k);
}
static int32_t msm_flash_config(struct msm_flash_ctrl_t *flash_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct msm_flash_cfg_data_t *flash_data =
		(struct msm_flash_cfg_data_t *) argp;

	mutex_lock(flash_ctrl->flash_mutex);

	CDBG("Enter %s type %d\n", __func__, flash_data->cfg_type);

	switch (flash_data->cfg_type) {
	case CFG_FLASH_INIT:
		rc = msm_flash_init_prepare(flash_ctrl, flash_data);
		break;
	case CFG_FLASH_RELEASE:
		 CDBG("CFG_FLASH_RELEASE");
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT){
			CDBG("call camera_flash_release");
			rc = flash_ctrl->func_tbl->camera_flash_release(
				flash_ctrl);
		}
		break;
	case CFG_FLASH_OFF:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_off(
				flash_ctrl, flash_data);
		break;
	case CFG_FLASH_LOW:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_low(
				flash_ctrl, flash_data);
		break;
	case CFG_FLASH_HIGH:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_high(
				flash_ctrl, flash_data);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(flash_ctrl->flash_mutex);

	CDBG("Exit %s type %d\n", __func__, flash_data->cfg_type);

	return rc;
}

static long msm_flash_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_flash_ctrl_t *fctrl = NULL;
	void __user *argp = (void __user *)arg;

	CDBG("Enter\n");

	if (!sd) {
		CDBG("sd NULL\n");
		return -EINVAL;
	}
	fctrl = v4l2_get_subdevdata(sd);
	if (!fctrl) {
		CDBG("fctrl NULL\n");
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_flash_get_subdev_id(fctrl, argp);
	case VIDIOC_MSM_FLASH_CFG:
		 CDBG("VIDIOC_MSM_FLASH_CFG\n");
		return msm_flash_config(fctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_UNNOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!fctrl->func_tbl) {
			CDBG("fctrl->func_tbl NULL\n");
			return -EINVAL;
		} else {
			CDBG("MSM_SD_SHUTDOWN camera_flash_release(\n");
			return fctrl->func_tbl->camera_flash_release(fctrl);
		}
	default:
		CDBG("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
	CDBG("Exit\n");
}

static struct v4l2_subdev_core_ops msm_flash_subdev_core_ops = {
	.ioctl = msm_flash_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_flash_subdev_ops = {
	.core = &msm_flash_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_flash_internal_ops;

#if 0
static int32_t msm_flash_get_pmic_source_info(
	struct device_node *of_node,
	struct msm_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;
	uint32_t count = 0, i = 0;
	struct device_node *flash_src_node = NULL;
	struct device_node *torch_src_node = NULL;
	struct device_node *switch_src_node = NULL;

	switch_src_node = of_parse_phandle(of_node, "qcom,switch-source", 0);
	if (!switch_src_node) {
		CDBG("%s:%d switch_src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_string(switch_src_node,
			"qcom,default-led-trigger",
			&fctrl->switch_trigger_name);
		if (rc < 0) {
			rc = of_property_read_string(switch_src_node,
				"linux,default-trigger",
				&fctrl->switch_trigger_name);
			if (rc < 0)
				CDBG("default-trigger read failed\n");
		}
		of_node_put(switch_src_node);
		switch_src_node = NULL;
		if (!rc) {
			CDBG("switch trigger %s\n",
				fctrl->switch_trigger_name);
			led_trigger_register_simple(
				fctrl->switch_trigger_name,
				&fctrl->switch_trigger);
		}
	}

	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			CDBG("invalid count\n");
			return -EINVAL;
		}
		fctrl->flash_num_sources = count;
		CDBG("%s:%d flash_num_sources = %d",
			__func__, __LINE__, fctrl->flash_num_sources);
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				CDBG("flash_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"qcom,default-led-trigger",
				&fctrl->flash_trigger_name[i]);
			if (rc < 0) {
				rc = of_property_read_string(flash_src_node,
					"linux,default-trigger",
					&fctrl->flash_trigger_name[i]);
				if (rc < 0) {
					CDBG("default-trigger read failed\n");
					of_node_put(flash_src_node);
					continue;
				}
			}

			CDBG("default trigger %s\n",
				fctrl->flash_trigger_name[i]);

			/* Read operational-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,current",
				&fctrl->flash_op_current[i]);
			if (rc < 0) {
				CDBG("current: read failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&fctrl->flash_max_current[i]);
			if (rc < 0) {
				CDBG("current: read failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-duration */
			rc = of_property_read_u32(flash_src_node,
				"qcom,duration",
				&fctrl->flash_max_duration[i]);
			if (rc < 0) {
				CDBG("duration: read failed\n");
				of_node_put(flash_src_node);
				/* Non-fatal; this property is optional */
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl->flash_op_current[i]);

			led_trigger_register_simple(
				fctrl->flash_trigger_name[i],
				&fctrl->flash_trigger[i]);
		}
		//if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
		//	fctrl->flash_driver_type = FLASH_DRIVER_PMIC;
		CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
			fctrl->flash_driver_type);
	}

	if (of_get_property(of_node, "qcom,torch-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			CDBG("invalid count\n");
			return -EINVAL;
		}
		fctrl->torch_num_sources = count;
		CDBG("%s:%d torch_num_sources = %d",
			__func__, __LINE__, fctrl->torch_num_sources);
		for (i = 0; i < count; i++) {
			torch_src_node = of_parse_phandle(of_node,
				"qcom,torch-source", i);
			if (!torch_src_node) {
				CDBG("torch_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(torch_src_node,
				"qcom,default-led-trigger",
				&fctrl->torch_trigger_name[i]);
			if (rc < 0) {
				rc = of_property_read_string(torch_src_node,
					"linux,default-trigger",
					&fctrl->torch_trigger_name[i]);
				if (rc < 0) {
					CDBG("default-trigger read failed\n");
					of_node_put(torch_src_node);
					continue;
				}
			}

			CDBG("default trigger %s\n",
				fctrl->torch_trigger_name[i]);

			/* Read operational-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,current",
				&fctrl->torch_op_current[i]);
			if (rc < 0) {
				CDBG("current: read failed\n");
				of_node_put(torch_src_node);
				continue;
			}

			/* Read max-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,max-current",
				&fctrl->torch_max_current[i]);
			if (rc < 0) {
				CDBG("current: read failed\n");
				of_node_put(torch_src_node);
				continue;
			}

			of_node_put(torch_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl->torch_op_current[i]);

			led_trigger_register_simple(
				fctrl->torch_trigger_name[i],
				&fctrl->torch_trigger[i]);
		}
		if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
			fctrl->flash_driver_type = FLASH_DRIVER_PMIC;
		CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
			fctrl->flash_driver_type);
	}

	return 0;
}
#endif

static int32_t msm_flash_get_dt_data(struct device_node *of_node,
	struct msm_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;

	
	//struct device_node *src_node = NULL;
	CDBG("called\n");

	if (!of_node) {
		CDBG("of_node NULL\n");
		return -EINVAL;
	}

	/* Read the sub device */
	rc = of_property_read_u32(of_node, "cell-index", &fctrl->pdev->id);
	if (rc < 0) {
		CDBG("failed rc %d\n", rc);
		return rc;
	}

	CDBG("subdev id %d\n", fctrl->subdev_id);

	fctrl->flash_driver_type = FLASH_DRIVER_I2C;
	
#if 0
	/* Read the CCI master. Use M0 if not available in the node */
	//if we use i2c,wo  set "qcom,cci-master" in dtsi
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&fctrl->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, fctrl->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		fctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	} else {
		fctrl->flash_driver_type = FLASH_DRIVER_I2C;
	}
#endif
	/* Read the flash and torch source info from device tree node */
	//This maybe not setting for lm36010
	#if 0
	rc = msm_flash_get_pmic_source_info(of_node, fctrl);
	if (rc < 0) {
		CDBG("%s:%d msm_flash_get_pmic_source_info failed rc %d\n",
			__func__, __LINE__, rc);
		return rc;
	}
	#endif

	/* Read the gpio information from device tree */
	//have gpio_flash_en and gpio_flash_now gpio control
	rc = msm_sensor_driver_get_gpio_data(
		&(fctrl->power_info.gpio_conf), of_node);
	if (rc < 0) {
		CDBG("%s:%d msm_sensor_driver_get_gpio_data failed rc %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
		fctrl->flash_driver_type = FLASH_DRIVER_GPIO;
	CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
		fctrl->flash_driver_type);

	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_flash_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t i = 0;
	int32_t rc = 0;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	struct msm_flash_cfg_data_t32 *u32;
	struct msm_flash_cfg_data_t flash_data;
	struct msm_flash_init_info_t32 flash_init_info32;
	struct msm_flash_init_info_t flash_init_info;

	CDBG("Enter");

	if (!file || !arg) {
		CDBG("%s:failed NULL parameter\n", __func__);
		return -EINVAL;
	}
	vdev = video_devdata(file);
	sd = vdev_to_v4l2_subdev(vdev);
	u32 = (struct msm_flash_cfg_data_t32 *)arg;

	flash_data.cfg_type = u32->cfg_type;
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		flash_data.flash_current[i] = u32->flash_current[i];
		flash_data.flash_duration[i] = u32->flash_duration[i];
	}
	CDBG("cmd is:%d",cmd);
	switch (cmd) {
	case VIDIOC_MSM_FLASH_CFG32:
		cmd = VIDIOC_MSM_FLASH_CFG;
		switch (flash_data.cfg_type) {
		case CFG_FLASH_OFF:
		case CFG_FLASH_LOW:
		case CFG_FLASH_HIGH:
			CDBG("flash_data.cfg_type=%d",flash_data.cfg_type);
			flash_data.cfg.settings = compat_ptr(u32->cfg.settings);
			break;
		case CFG_FLASH_INIT:
			flash_data.cfg.flash_init_info = &flash_init_info;
			if (copy_from_user(&flash_init_info32,
				(void *)compat_ptr(u32->cfg.flash_init_info),
				sizeof(struct msm_flash_init_info_t32))) {
				CDBG("%s copy_from_user failed %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
			flash_init_info.flash_driver_type =
				flash_init_info32.flash_driver_type;
			flash_init_info.slave_addr =
				flash_init_info32.slave_addr;
			flash_init_info.i2c_freq_mode =
				flash_init_info32.i2c_freq_mode;
			flash_init_info.settings =
				compat_ptr(flash_init_info32.settings);
			flash_init_info.power_setting_array =
				compat_ptr(
				flash_init_info32.power_setting_array);
			break;
		default:
			break;
		}
		break;
	default:
		return msm_flash_subdev_ioctl(sd, cmd, arg);
	}

	rc =  msm_flash_subdev_ioctl(sd, cmd, &flash_data);
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		u32->flash_current[i] = flash_data.flash_current[i];
		u32->flash_duration[i] = flash_data.flash_duration[i];
	}
	CDBG("Exit");
	return rc;
}

static long msm_flash_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_flash_subdev_do_ioctl);
}
#endif

static int lm36010_reg_init(struct regmap *regmap)
{
	int ret;

	/* set enable register */
	ret = regmap_update_bits(regmap, REG_ENABLE, EX_PIN_ENABLE_MASK,0);
	if (ret < 0)
		CDBG("Failed to update REG_ENABLE Register\n");
	return ret;
}

static int32_t msm_flash_lm36010_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_flash_ctrl_t *flash_ctrl = NULL;

	CDBG("Enter");
	if (!pdev->dev.of_node) {
		CDBG("of_node NULL\n");
		return -EINVAL;
	}

	flash_ctrl = kzalloc(sizeof(struct msm_flash_ctrl_t), GFP_KERNEL);
	if (!flash_ctrl) {
		CDBG("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	memset(flash_ctrl, 0, sizeof(struct msm_flash_ctrl_t));

	flash_ctrl->pdev = pdev;

	rc = msm_flash_get_dt_data(pdev->dev.of_node, flash_ctrl);
	if (rc < 0) {
		CDBG("%s:%d msm_flash_get_dt_data failed\n",
			__func__, __LINE__);
		kfree(flash_ctrl);
		return -EINVAL;
	}

	flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;
	flash_ctrl->power_info.dev = &flash_ctrl->pdev->dev;
	flash_ctrl->flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	flash_ctrl->flash_mutex = &msm_flash_mutex;

	
	//flash_ctrl->flash_i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	#if 0
	flash_ctrl->flash_i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	flash_ctrl->flash_i2c_client.cci_client = kzalloc(
		sizeof(struct msm_camera_cci_client), GFP_KERNEL);
	if (!flash_ctrl->flash_i2c_client.cci_client) {
		kfree(flash_ctrl);
		CDBG("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = flash_ctrl->flash_i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = flash_ctrl->cci_i2c_master;
	#endif
	flash_ctrl->flash_i2c_client.client=lm36010_i2c_client;
	/* Initialize sub device */
	v4l2_subdev_init(&flash_ctrl->msm_sd.sd, &msm_flash_subdev_ops);
	v4l2_set_subdevdata(&flash_ctrl->msm_sd.sd, flash_ctrl);

	flash_ctrl->msm_sd.sd.internal_ops = &msm_flash_internal_ops;
	flash_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(flash_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(flash_ctrl->msm_sd.sd.name),
		"msm_camera_flash");
	media_entity_init(&flash_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	flash_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	flash_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_FLASH;
	flash_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;
	msm_sd_register(&flash_ctrl->msm_sd);

	CDBG("%s:%d flash sd name = %s", __func__, __LINE__,
		flash_ctrl->msm_sd.sd.entity.name);
	msm_cam_copy_v4l2_subdev_fops(&msm_flash_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_flash_v4l2_subdev_fops.compat_ioctl32 =
		msm_flash_subdev_fops_ioctl;
#endif
	flash_ctrl->msm_sd.sd.devnode->fops = &msm_flash_v4l2_subdev_fops;


	//if (flash_ctrl->flash_driver_type == FLASH_DRIVER_PMIC)
	//	rc = msm_torch_create_classdev(pdev, flash_ctrl);
	platform_set_drvdata(pdev, flash_ctrl);
	CDBG("ftm mode will have led class node\r\n");
  	if(flash_ctrl->flash_driver_type == FLASH_DRIVER_I2C){
		mutex_init(&g_lm36010_data.lock);
		g_lm36010_data.regmap=devm_regmap_init_i2c(lm36010_i2c_client, &lm36010_regmap);
		if(lm36010_reg_init(g_lm36010_data.regmap)<0)
			goto error_out;
		i2c_set_clientdata(lm36010_i2c_client, &g_lm36010_data);

		//if(strstr(saved_command_line, "androidboot.mode=2")!=NULL){
		
			INIT_WORK(&g_lm36010_data.work_torch, lm36010_torch_brightness_set_work);
			g_lm36010_data.cdev_torch.name = "led:torch_0";
			g_lm36010_data.cdev_torch.max_brightness = 255;
			g_lm36010_data.cdev_torch.brightness_set = lm36010_torch_brightness_set;
	
		   rc = led_classdev_register(&(flash_ctrl->flash_i2c_client.client->dev), &g_lm36010_data.cdev_torch);
			if (rc < 0) {
				CDBG("led_classdev_register error\n");
				goto error_out_cdev_torch;
			}
			INIT_WORK(&g_lm36010_data.work_switch, lm36010_switch_brightness_set_work);
			g_lm36010_data.cdev_switch.name = "led:switch";
			g_lm36010_data.cdev_switch.max_brightness = 255;
			g_lm36010_data.cdev_switch.brightness_set = lm36010_switch_brightness_set;
			rc = led_classdev_register(&(flash_ctrl->flash_i2c_client.client->dev), &g_lm36010_data.cdev_switch);
			if (rc < 0) {
				CDBG("led_classdev_register error\n");
				goto error_out_cdev_switch;
			}
		//}
		//}
	//msm_lm36010_flash_create_classdev(&pdev->dev,flash_ctrl);
	//msm_lm36010_flash_create_classdev(&(flash_ctrl->flash_i2c_client.client->dev),flash_ctrl);
	}

	CDBG("probe success\n");
	return rc;
	error_out_cdev_switch:
	led_classdev_unregister(&g_lm36010_data.cdev_torch);
	error_out_cdev_torch:
	error_out:
		if(flash_ctrl){
			msm_sd_unregister(&flash_ctrl->msm_sd);
			kfree(flash_ctrl);
		}
	return -1;
		
}

static void msm_flash_lm36010_platform_shutdown(struct platform_device *pdev)
{
	struct msm_flash_ctrl_t *flash_ctrl = platform_get_drvdata(pdev);
	CDBG("into");
	if(flash_ctrl){
		CDBG("off flash led");
		msm_flash_i2c_off_current(flash_ctrl);
	}
	else{

	}
	CDBG("exit");
}
MODULE_DEVICE_TABLE(of, msm_flash_dt_match);

static struct platform_driver msm_flash_lm36010_platform_driver = {
	.probe = msm_flash_lm36010_platform_probe,
	.driver = {
		.name = "qcom,leds-lm36010",
		.owner = THIS_MODULE,
		.of_match_table = msm_flash_dt_match,
	},
	.shutdown	= msm_flash_lm36010_platform_shutdown,
};

 static int lm36010_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	
	 CDBG("into");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			dev_err(&client->dev, "i2c functionality check fail.\n");
			return -EOPNOTSUPP;
		}
	CDBG("success");
	lm36010_i2c_client=client;
	return 0;
	
}

static int lm36010_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id lm36010_id[] = {
	{LM36010_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm36010_id);



static const struct of_device_id leds_lm36010_dt_match[] = {
	{.compatible = "qcom,i2c_leds-lm36010", .data = NULL},
	{}
};

static struct i2c_driver lm36010_i2c_driver = {
	.driver = {
		   .name = LM36010_NAME,
		   .owner = THIS_MODULE,
		   .pm = NULL,
		   .of_match_table = leds_lm36010_dt_match,
		   },
	.probe = lm36010_probe,
	.remove = lm36010_remove,
	.id_table = lm36010_id,
};

static int __init msm_flash_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc=i2c_register_driver(THIS_MODULE,&lm36010_i2c_driver);
	if(rc<0)
		CDBG("i2c_register_driver failed");
	rc = platform_driver_register(&msm_flash_lm36010_platform_driver);
	if (rc<0){
		
		CDBG("platform probe for flash failed");
		i2c_del_driver(&lm36010_i2c_driver);
	}

	CDBG("success\n");
	return rc;
}

static void __exit msm_flash_exit_module(void)
{
	platform_driver_unregister(&msm_flash_lm36010_platform_driver);
	i2c_del_driver(&lm36010_i2c_driver);
	return;
}

static struct msm_flash_table msm_pmic_flash_table = {
	.flash_driver_type = FLASH_DRIVER_PMIC,
	.func_tbl = {
		.camera_flash_init = NULL,
		.camera_flash_release = msm_flash_release,
		.camera_flash_off = msm_flash_off,
		.camera_flash_low = msm_flash_low,
		.camera_flash_high = msm_flash_high,
	},
};

static struct msm_flash_table msm_gpio_flash_table = {
	.flash_driver_type = FLASH_DRIVER_GPIO,
	.func_tbl = {
		.camera_flash_init = msm_flash_gpio_init,
		.camera_flash_release = msm_flash_release,
		.camera_flash_off = msm_flash_off,
		.camera_flash_low = msm_flash_low,
		.camera_flash_high = msm_flash_high,
	},
};

static struct msm_flash_table msm_i2c_flash_table = {
	.flash_driver_type = FLASH_DRIVER_I2C,
	.func_tbl = {
		.camera_flash_init = msm_flash_i2c_init,
		.camera_flash_release = msm_flash_i2c_release,
		.camera_flash_off = msm_flash_off_i2c_write_setting_array,
		.camera_flash_low = msm_flash_low_i2c_write_setting_array,
		.camera_flash_high = msm_flash_high_i2c_write_setting_array,
	},
};

module_init(msm_flash_init_module);
module_exit(msm_flash_exit_module);
MODULE_DESCRIPTION("MSM FLASH");
MODULE_LICENSE("GPL v2");


